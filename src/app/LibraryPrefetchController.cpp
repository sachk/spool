#include "LibraryPrefetchController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"

#include <QDebug>

#include <algorithm>
#include <utility>

namespace JellyfinNative {

namespace {

constexpr int kLibraryPageSize = 100;
constexpr int kBackgroundLibraryPrefetchLimit = 3;

bool showsLatestLibraryRow(const LibraryItem &library) {
  static const QSet<QString> excluded = {
      QStringLiteral("playlists"), QStringLiteral("livetv"),
      QStringLiteral("boxsets"),   QStringLiteral("channels"),
      QStringLiteral("folders"),
  };
  return !library.id.isEmpty() && !excluded.contains(library.collectionType);
}

QString cacheKey(const LibraryItem &library) {
  if (library.collectionType == QStringLiteral("tvshows"))
    return QStringLiteral("series/%1").arg(library.id);
  if (library.collectionType == QStringLiteral("movies"))
    return library.id;
  return QStringLiteral("library/%1/%2")
      .arg(library.collectionType, library.id);
}

} // namespace

LibraryPrefetchController::LibraryPrefetchController(JellyfinApiFacade *api,
                                                     QObject *parent)
    : QObject(parent), m_api(api) {
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this,
          &LibraryPrefetchController::startNext);
}

void LibraryPrefetchController::stop() {
  m_generation.invalidate();
  m_timer.stop();
  m_queue.clear();
  m_index = 0;
  m_active = false;
}

void LibraryPrefetchController::schedule(
    const std::vector<LibraryItem> &libraries,
    const QStringList &recentLibraryIds) {
  stop();
  if (!m_api || m_api->session().accessToken.isEmpty())
    return;

  std::vector<LibraryItem> selected;
  selected.reserve(kBackgroundLibraryPrefetchLimit);
  QSet<QString> selectedIds;

  auto trySelect = [&selected, &selectedIds](const LibraryItem &library) {
    if (selected.size() >= static_cast<size_t>(kBackgroundLibraryPrefetchLimit))
      return;
    if (!showsLatestLibraryRow(library) || selectedIds.contains(library.id))
      return;
    selectedIds.insert(library.id);
    selected.push_back(library);
  };

  for (const QString &recentId : recentLibraryIds) {
    const auto it = std::find_if(libraries.begin(), libraries.end(),
                                 [&recentId](const LibraryItem &library) {
                                   return library.id == recentId;
                                 });
    if (it != libraries.end())
      trySelect(*it);
  }
  for (const LibraryItem &library : libraries)
    trySelect(library);

  QSet<QString> retainedKeys;
  for (const LibraryItem &library : selected) {
    const QString key = cacheKey(library);
    retainedKeys.insert(key);
    if (!m_cachedKeys.contains(key))
      m_queue.push_back(library);
  }

  for (auto it = m_pages.begin(); it != m_pages.end();) {
    if (retainedKeys.contains(it.key()))
      ++it;
    else
      it = m_pages.erase(it);
  }
  m_cachedKeys.intersect(retainedKeys);

  if (m_queue.empty())
    return;

  qInfo() << "library prefetch: scheduled" << m_queue.size()
          << "libraries after home load";
  m_timer.start(1500);
}

std::optional<PagedMovieItems>
LibraryPrefetchController::cachedPage(const QString &cacheKey) const {
  const auto it = m_pages.constFind(cacheKey);
  if (it == m_pages.constEnd())
    return std::nullopt;
  return it.value();
}

void LibraryPrefetchController::prefetchPosters(
    const std::vector<MovieItem> &items, int firstIndex, int visibleCount) {
  if (items.empty())
    return;

  const int begin = std::clamp(firstIndex, 0, static_cast<int>(items.size()));
  const int windowSize = std::max(1, visibleCount) + m_imagePrefetchAheadItems;
  const int end =
      std::min(static_cast<int>(items.size()), begin + windowSize);
  QStringList urls;
  urls.reserve((end - begin) * 2);

  for (int index = begin; index < end; ++index) {
    const MovieItem &item = items[static_cast<size_t>(index)];
    if (!item.posterUrl.isEmpty())
      urls.push_back(item.posterUrl);
    if (!item.backdropUrl.isEmpty())
      urls.push_back(item.backdropUrl);
    if (!item.logoUrl.isEmpty())
      urls.push_back(item.logoUrl);
  }

  if (!urls.isEmpty())
    m_api->prefetchImages(urls, m_imagePrefetchMaxConcurrent);
}

void LibraryPrefetchController::configureImagePrefetch(int aheadItems,
                                                       int maxConcurrent) {
  m_imagePrefetchAheadItems = std::clamp(aheadItems, 4, 64);
  m_imagePrefetchMaxConcurrent = std::clamp(maxConcurrent, 1, 8);
}

void LibraryPrefetchController::startNext() {
  if (m_active || !m_api || m_api->session().accessToken.isEmpty())
    return;
  if (m_index < 0 || m_index >= static_cast<int>(m_queue.size()))
    return;

  const RequestGeneration::Token generation = m_generation.next();
  const LibraryItem library = m_queue[static_cast<size_t>(m_index++)];
  const QString key = cacheKey(library);
  m_active = true;
  qInfo() << "library prefetch: fetching" << library.name << key;

  Async::runLatest(
      this,
      m_api->fetchLibraryPage(library.id, library.collectionType, 0,
                              kLibraryPageSize),
      m_generation, generation,
      [this, key, library](const PagedMovieItems &page) {
        qInfo() << "library prefetch: cached" << library.name
                << page.items.size();
        m_pages.insert(key, page);
        m_cachedKeys.insert(key);
        m_active = false;
        startNext();
      },
      [this, key, library](const std::exception_ptr &error) {
        qWarning() << "library prefetch: failed" << library.name << key
                   << exceptionMessage(error);
        m_active = false;
        startNext();
      });
}

} // namespace JellyfinNative

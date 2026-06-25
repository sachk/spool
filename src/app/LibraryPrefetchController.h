#pragma once

#include "../common/JellyfinTypes.h"
#include "../common/RequestGeneration.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <optional>
#include <vector>

namespace JellyfinNative {

class JellyfinApiFacade;

class LibraryPrefetchController final : public QObject {
  Q_OBJECT

public:
  enum class ImageKind { Poster, Landscape };

  explicit LibraryPrefetchController(JellyfinApiFacade *api,
                                     QObject *parent = nullptr);

  void stop();
  void schedule(const std::vector<LibraryItem> &libraries,
                const QStringList &recentLibraryIds);
  std::optional<PagedMovieItems> cachedPage(const QString &cacheKey) const;
  void configureImagePrefetch(int aheadItems, int maxConcurrent);
  void prefetchPosters(const std::vector<MovieItem> &items, int firstIndex = 0,
                       int visibleCount = 12,
                       ImageKind imageKind = ImageKind::Poster);

private:
  void startNext();

  JellyfinApiFacade *m_api = nullptr;
  QTimer m_timer;
  RequestGeneration m_generation;
  int m_index = 0;
  bool m_active = false;
  std::vector<LibraryItem> m_queue;
  QHash<QString, PagedMovieItems> m_pages;
  QSet<QString> m_cachedKeys;
  int m_imagePrefetchAheadItems = 16;
  int m_imagePrefetchMaxConcurrent = 3;
};

} // namespace JellyfinNative

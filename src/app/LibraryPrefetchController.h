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
  explicit LibraryPrefetchController(JellyfinApiFacade *api,
                                     QObject *parent = nullptr);

  void stop();
  void schedule(const std::vector<LibraryItem> &libraries,
                const QStringList &recentLibraryIds);
  std::optional<PagedMovieItems> cachedPage(const QString &cacheKey) const;
  void prefetchPosters(const std::vector<MovieItem> &items);

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
};

} // namespace JellyfinNative

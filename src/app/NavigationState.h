#pragma once

#include "../common/JellyfinTypes.h"

#include <QHash>
#include <QString>
#include <QVariantMap>

namespace JellyfinNative {

class NavigationState final {
public:
  QString page() const;
  QString libraryId() const;
  QString libraryCollectionType() const;
  QString title() const;
  QString contentLabel() const;
  QString viewKind() const;
  QString seriesId() const;
  QString seriesName() const;
  QString seasonId() const;
  QVariantMap query() const;
  QVariantMap filterOptions() const;
  BrowseDescriptor browseDescriptor() const;

  void reset();
  bool setPage(const QString &page);
  void enterLibrary(const LibraryItem &library, const QString &contentLabel,
                    const QVariantMap &defaultQuery);
  void enterSeries(const MovieItem &series);
  void enterSeason(const QString &seriesId, const MovieItem &season);
  void enterNamedCollection(const QString &viewKind, const QString &name);
  void enterPlaylist(const MovieItem &playlist);
  void enterBoxSet(const MovieItem &boxSet);
  void enterFolder(const MovieItem &folder);
  bool setQuery(const QVariantMap &query);
  void clearFilterOptions();
  void setFilterOptions(const QVariantMap &options);

private:
  QString m_page = QStringLiteral("login");
  QString m_libraryId;
  QString m_libraryCollectionType;
  QString m_title;
  QString m_contentLabel = QStringLiteral("Movies");
  QString m_viewKind;
  QString m_seriesId;
  QString m_seriesName;
  QString m_seasonId;
  BrowseDescriptor m_browseDescriptor;
  QHash<QString, QVariantMap> m_libraryQueries;
  QVariantMap m_query;
  QVariantMap m_filterOptions;
};

} // namespace JellyfinNative

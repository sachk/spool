#include "NavigationState.h"

namespace JellyfinNative {

QString NavigationState::page() const { return m_page; }

QString NavigationState::libraryId() const { return m_libraryId; }

QString NavigationState::libraryCollectionType() const {
  return m_libraryCollectionType;
}

QString NavigationState::title() const { return m_title; }

QString NavigationState::contentLabel() const { return m_contentLabel; }

QString NavigationState::viewKind() const { return m_viewKind; }

QString NavigationState::seriesId() const { return m_seriesId; }

QString NavigationState::seriesName() const { return m_seriesName; }

QString NavigationState::seasonId() const { return m_seasonId; }

QVariantMap NavigationState::query() const { return m_query; }

QVariantMap NavigationState::filterOptions() const { return m_filterOptions; }

void NavigationState::reset() {
  m_page = QStringLiteral("login");
  m_libraryId.clear();
  m_libraryCollectionType.clear();
  m_title.clear();
  m_contentLabel = QStringLiteral("Movies");
  m_viewKind.clear();
  m_seriesId.clear();
  m_seriesName.clear();
  m_seasonId.clear();
  m_libraryQueries.clear();
  m_query.clear();
  m_filterOptions.clear();
}

bool NavigationState::setPage(const QString &page) {
  if (m_page == page)
    return false;
  m_page = page;
  return true;
}

void NavigationState::enterLibrary(const LibraryItem &library,
                                   const QString &contentLabel,
                                   const QVariantMap &defaultQuery) {
  m_libraryId = library.id;
  m_libraryCollectionType = library.collectionType;
  m_title = library.name;
  m_contentLabel = contentLabel;
  m_viewKind = QStringLiteral("library");
  m_seriesId.clear();
  m_seriesName.clear();
  m_seasonId.clear();
  m_query = m_libraryQueries.value(library.id, defaultQuery);
  m_filterOptions.clear();
}

void NavigationState::enterSeries(const MovieItem &series) {
  m_seriesId = series.id;
  m_seriesName = series.title;
  m_seasonId.clear();
  m_viewKind = QStringLiteral("seasons");
  m_title = series.title;
  m_contentLabel = QStringLiteral("Seasons");
}

void NavigationState::enterSeason(const QString &seriesId,
                                  const MovieItem &season) {
  m_seriesId = seriesId;
  m_seasonId =
      season.itemType == QStringLiteral("Season") ? season.id : QString();
  m_viewKind = QStringLiteral("episodes");
  m_title = season.title;
  m_contentLabel = QStringLiteral("Episodes");
}

void NavigationState::enterNamedCollection(const QString &viewKind,
                                           const QString &name) {
  m_libraryId.clear();
  m_libraryCollectionType.clear();
  m_seriesId.clear();
  m_seriesName.clear();
  m_seasonId.clear();
  m_viewKind = viewKind;
  m_title = name;
  m_contentLabel = QStringLiteral("Titles");
  m_query.clear();
  m_filterOptions.clear();
}

bool NavigationState::setQuery(const QVariantMap &query) {
  if (m_query == query)
    return false;
  m_query = query;
  if (!m_libraryId.isEmpty())
    m_libraryQueries.insert(m_libraryId, m_query);
  return true;
}

void NavigationState::clearFilterOptions() { m_filterOptions.clear(); }

void NavigationState::setFilterOptions(const QVariantMap &options) {
  m_filterOptions = options;
}

} // namespace JellyfinNative

#include "UserItemStateController.h"

#include "BrowseSessionController.h"
#include "ContentModelController.h"
#include "HomeModelController.h"
#include "SearchController.h"

namespace JellyfinNative {

UserItemStateController::UserItemStateController(BrowseSessionController *currentItems, HomeModelController *home,
    ContentModelController *content, SearchController *search, QObject *parent)
    : QObject(parent)
    , m_browse(currentItems)
    , m_home(home)
    , m_content(content)
    , m_search(search)
{
}

void UserItemStateController::applyResumeTicks(const QString& itemId, qint64 positionTicks)
{
    if (itemId.isEmpty() || positionTicks < 0)
        return;

    if (m_browse)
        m_browse->updateResumeTicks(itemId, positionTicks);
    if (m_home)
        m_home->updateResumeTicks(itemId, positionTicks);
    if (m_content)
        m_content->updateResumeTicks(itemId, positionTicks);
    if (m_search)
        m_search->updateResumeTicks(itemId, positionTicks);
}

void UserItemStateController::applyFavorite(const QString& itemId, bool favorite)
{
    if (itemId.isEmpty())
        return;

    if (m_browse)
        m_browse->updateFavorite(itemId, favorite);
    if (m_home)
        m_home->updateFavorite(itemId, favorite);
    if (m_content)
        m_content->updateFavorite(itemId, favorite);
    if (m_search)
        m_search->updateFavorite(itemId, favorite);
    emit favoriteChanged(itemId, favorite);
}

void UserItemStateController::applyPlayed(const QString& itemId, bool played)
{
    if (itemId.isEmpty())
        return;

    if (m_browse)
        m_browse->updatePlayed(itemId, played);
    if (m_home)
        m_home->updatePlayed(itemId, played);
    if (m_content)
        m_content->updatePlayed(itemId, played);
    if (m_search)
        m_search->updatePlayed(itemId, played);
    emit playedChanged(itemId, played);
}

} // namespace JellyfinNative

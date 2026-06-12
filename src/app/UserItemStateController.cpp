#include "UserItemStateController.h"

#include "ContentModelController.h"
#include "CurrentItemsController.h"
#include "HomeModelController.h"

namespace JellyfinNative {

UserItemStateController::UserItemStateController(
    CurrentItemsController *currentItems, HomeModelController *home,
    ContentModelController *content, QObject *parent)
    : QObject(parent)
    , m_currentItems(currentItems)
    , m_home(home)
    , m_content(content)
{
}

void UserItemStateController::applyResumeTicks(const QString &itemId,
                                               qint64 positionTicks)
{
    if (itemId.isEmpty() || positionTicks < 0)
        return;

    if (m_currentItems)
        m_currentItems->updateResumeTicks(itemId, positionTicks);
    if (m_home)
        m_home->updateResumeTicks(itemId, positionTicks);
    if (m_content)
        m_content->updateResumeTicks(itemId, positionTicks);
}

void UserItemStateController::applyFavorite(const QString &itemId,
                                            bool favorite)
{
    if (itemId.isEmpty())
        return;

    if (m_currentItems)
        m_currentItems->updateFavorite(itemId, favorite);
    if (m_home)
        m_home->updateFavorite(itemId, favorite);
    if (m_content)
        m_content->updateFavorite(itemId, favorite);
    emit favoriteChanged(itemId, favorite);
}

void UserItemStateController::applyPlayed(const QString &itemId, bool played)
{
    if (itemId.isEmpty())
        return;

    if (m_currentItems)
        m_currentItems->updatePlayed(itemId, played);
    if (m_home)
        m_home->updatePlayed(itemId, played);
    if (m_content)
        m_content->updatePlayed(itemId, played);
    emit playedChanged(itemId, played);
}

} // namespace JellyfinNative

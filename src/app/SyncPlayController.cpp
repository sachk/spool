#include "SyncPlayController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/JellyfinTypes.h"
#include "../player/PlayerController.h"

#include <QCoroTask>

#include <QDebug>
#include <QJsonObject>

namespace JellyfinNative {

SyncPlayController::SyncPlayController(JellyfinApiFacade *api, PlayerController *player, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_player(player)
{
}

void SyncPlayController::refreshGroups()
{
    if (!m_api)
        return;
    QCoro::runDetached(
        m_api->fetchSyncPlayGroups(),
        [this](const QJsonArray &groups) {
            m_groups = groups;
            emit groupsChanged();
        },
        [this](const std::exception_ptr &error) {
            emit errorText(exceptionMessage(error));
        });
}

void SyncPlayController::createGroup(const QString &name)
{
    if (!m_api)
        return;
    QCoro::runDetached(
        m_api->createSyncPlayGroup(name),
        [this]() { refreshGroups(); },
        [this](const std::exception_ptr &error) { emit errorText(exceptionMessage(error)); });
}

void SyncPlayController::joinGroup(const QString &groupId)
{
    if (!m_api || groupId.isEmpty())
        return;
    QCoro::runDetached(
        m_api->joinSyncPlayGroup(groupId),
        [this, groupId]() {
            m_groupId = groupId;
            for (const auto &value : m_groups) {
                const QJsonObject obj = value.toObject();
                if (obj.value(QStringLiteral("GroupId")).toString() == groupId) {
                    m_groupName = obj.value(QStringLiteral("GroupName")).toString();
                    break;
                }
            }
            emit groupChanged();
        },
        [this](const std::exception_ptr &error) { emit errorText(exceptionMessage(error)); });
}

void SyncPlayController::leaveGroup()
{
    if (!m_api || m_groupId.isEmpty())
        return;
    QCoro::runDetached(
        m_api->leaveSyncPlayGroup(),
        [this]() {
            m_groupId.clear();
            m_groupName.clear();
            emit groupChanged();
        },
        [this](const std::exception_ptr &error) { emit errorText(exceptionMessage(error)); });
}

void SyncPlayController::requestPause()
{
    if (!m_api || m_groupId.isEmpty())
        return;
    QCoro::runDetached(m_api->syncPlayRequestPause(),
                       []() {}, [](const std::exception_ptr &) {});
}

void SyncPlayController::requestPlay()
{
    if (!m_api || m_groupId.isEmpty())
        return;
    QCoro::runDetached(m_api->syncPlayRequestPlay(),
                       []() {}, [](const std::exception_ptr &) {});
}

void SyncPlayController::requestSeekSeconds(double seconds)
{
    if (!m_api || m_groupId.isEmpty())
        return;
    const qint64 ticks = static_cast<qint64>(seconds * 10000000.0);
    QCoro::runDetached(m_api->syncPlayRequestSeek(ticks),
                       []() {}, [](const std::exception_ptr &) {});
}

} // namespace JellyfinNative

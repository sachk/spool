#include "LibraryManagementController.h"

#include "../api/JellyfinApiFacade.h"
#include "../common/AsyncTask.h"
#include "../common/MetaJson.h"
#include "BrowseSessionController.h"

#include <QDebug>
#include <QJsonObject>
#include <QVariantMap>

#include <algorithm>

namespace JellyfinNative {

LibraryManagementController::LibraryManagementController(
    JellyfinApiFacade *api, BrowseSessionController *browse, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_browse(browse)
{
}

void LibraryManagementController::clear()
{
    m_playlistTargets.clear();
    m_collectionTargets.clear();
    m_policyLoadStarted = false;
    m_currentUserCanManagePlaylists = false;
    m_currentUserCanManageCollections = false;
    m_currentUserCanRenameItems = false;
    m_currentUserCanDeleteItems = false;
    emit policyChanged();
    emit targetsChanged();
}

void LibraryManagementController::loadCurrentUserPolicy()
{
    if (m_policyLoadStarted)
        return;
    if (!m_api || m_api->session().accessToken.isEmpty()) {
        clear();
        return;
    }
    m_policyLoadStarted = true;

    m_currentUserCanManagePlaylists = true;
    emit policyChanged();
    Async::runScoped(
        this, m_api->fetchCurrentUserPolicy(),
        [this](const QJsonObject& policy) {
            const bool administrator = policy.value(QStringLiteral("IsAdministrator")).toBool(false);
            m_currentUserCanManagePlaylists = !m_api->session().accessToken.isEmpty();
            m_currentUserCanManageCollections
                = administrator || policy.value(QStringLiteral("EnableCollectionManagement")).toBool(false);
            m_currentUserCanRenameItems = administrator;
            m_currentUserCanDeleteItems
                = administrator || policy.value(QStringLiteral("EnableContentDeletion")).toBool(false);
            emit policyChanged();
        },
        [this](const std::exception_ptr& error) {
            qWarning() << "management policy fetch failed" << exceptionMessage(error);
            m_currentUserCanManagePlaylists = false;
            m_currentUserCanManageCollections = false;
            m_currentUserCanRenameItems = false;
            m_currentUserCanDeleteItems = false;
            emit policyChanged();
        });
}

void LibraryManagementController::setTargets(const QString& kind, const std::vector<MovieItem>& items)
{
    QVariantList targets;
    targets.reserve(static_cast<qsizetype>(items.size()));
    for (const MovieItem& item : items)
        targets.push_back(metaToJson(item).toVariantMap());

    if (kind == QStringLiteral("playlist"))
        m_playlistTargets = targets;
    else if (kind == QStringLiteral("collection"))
        m_collectionTargets = targets;
    emit targetsChanged();
}

QStringList LibraryManagementController::itemIdsFor(const MovieItem& item) const
{
    return item.id.isEmpty() ? QStringList {} : QStringList { item.id };
}

void LibraryManagementController::refreshAfterMutation(const QString& changedItemId)
{
    emit refreshRequested(changedItemId);
    refreshTargets(QStringLiteral("playlist"));
    refreshTargets(QStringLiteral("collection"));
}

bool LibraryManagementController::authenticated()
{
    if (!m_api || m_api->session().accessToken.isEmpty()) {
        emit errorOccurred(QStringLiteral("Sign in before managing library items."));
        return false;
    }
    return true;
}

bool LibraryManagementController::playlistAllowed()
{
    if (!authenticated())
        return false;
    if (!m_currentUserCanManagePlaylists) {
        emit errorOccurred(QStringLiteral("Your Jellyfin user cannot manage playlists."));
        return false;
    }
    return true;
}

bool LibraryManagementController::collectionAllowed()
{
    if (!authenticated())
        return false;
    if (!m_currentUserCanManageCollections) {
        emit errorOccurred(QStringLiteral("Your Jellyfin user cannot manage collections."));
        return false;
    }
    return true;
}

bool LibraryManagementController::renameAllowed(const QString& itemType)
{
    if (itemType == QStringLiteral("Playlist"))
        return playlistAllowed();
    if (!authenticated())
        return false;
    if (!m_currentUserCanRenameItems) {
        emit errorOccurred(QStringLiteral("Your Jellyfin user cannot rename this item."));
        return false;
    }
    return true;
}

bool LibraryManagementController::deleteAllowed()
{
    if (!authenticated())
        return false;
    if (!m_currentUserCanDeleteItems) {
        emit errorOccurred(QStringLiteral("Your Jellyfin user cannot delete items."));
        return false;
    }
    return true;
}

void LibraryManagementController::refreshTargets(const QString& kind)
{
    if (!authenticated())
        return;

    const QString normalized
        = kind == QStringLiteral("collection") ? QStringLiteral("collection") : QStringLiteral("playlist");
    const QString itemType
        = normalized == QStringLiteral("collection") ? QStringLiteral("BoxSet") : QStringLiteral("Playlist");
    Async::runScoped(
        this, m_api->fetchManagementTargets(itemType),
        [this, normalized](const std::vector<MovieItem>& items) { setTargets(normalized, items); },
        [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); });
}

void LibraryManagementController::createPlaylistForItem(const QString& name, const MovieItem& item)
{
    if (!playlistAllowed())
        return;
    const QStringList itemIds = itemIdsFor(item);
    Async::runScoped(
        this, m_api->createPlaylist(name, itemIds),
        [this](const QString&) {
            emit operationSucceeded(QStringLiteral("Playlist created"));
            refreshAfterMutation();
        },
        [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); });
}

void LibraryManagementController::addItemToPlaylist(const QString& playlistId, const MovieItem& item)
{
    if (!playlistAllowed())
        return;
    const QStringList itemIds = itemIdsFor(item);
    if (playlistId.isEmpty() || itemIds.isEmpty()) {
        emit errorOccurred(QStringLiteral("Choose an item and playlist first."));
        return;
    }
    Async::runScoped(
        this, m_api->addPlaylistItems(playlistId, itemIds),
        [this]() {
            emit operationSucceeded(QStringLiteral("Added to playlist"));
            refreshAfterMutation();
        },
        [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); });
}

void LibraryManagementController::createCollectionForItem(const QString& name, const MovieItem& item)
{
    if (!collectionAllowed())
        return;
    const QStringList itemIds = itemIdsFor(item);
    Async::runScoped(
        this, m_api->createCollection(name, itemIds),
        [this](const QString&) {
            emit operationSucceeded(QStringLiteral("Collection created"));
            refreshAfterMutation();
        },
        [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); });
}

void LibraryManagementController::addItemToCollection(const QString& collectionId, const MovieItem& item)
{
    if (!collectionAllowed())
        return;
    const QStringList itemIds = itemIdsFor(item);
    if (collectionId.isEmpty() || itemIds.isEmpty()) {
        emit errorOccurred(QStringLiteral("Choose an item and collection first."));
        return;
    }
    Async::runScoped(
        this, m_api->addCollectionItems(collectionId, itemIds),
        [this]() {
            emit operationSucceeded(QStringLiteral("Added to collection"));
            refreshAfterMutation();
        },
        [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); });
}

void LibraryManagementController::removeItemFromCurrentParent(const MovieItem& movie)
{
    if (!m_browse) {
        emit errorOccurred(QStringLiteral("Open a playlist or collection before removing items."));
        return;
    }

    const BrowseDescriptor descriptor = m_browse->descriptor();
    if (descriptor.kind == BrowseKind::Playlist) {
        if (!playlistAllowed())
            return;
        if (movie.playlistItemId.isEmpty()) {
            emit errorOccurred(QStringLiteral("This playlist entry cannot be removed."));
            return;
        }
        Async::runScoped(
            this, m_api->removePlaylistItems(descriptor.id, { movie.playlistItemId }),
            [this]() {
                emit operationSucceeded(QStringLiteral("Removed from playlist"));
                refreshAfterMutation();
            },
            [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); });
        return;
    }
    if (descriptor.kind == BrowseKind::BoxSet) {
        if (!collectionAllowed())
            return;
        if (movie.id.isEmpty()) {
            emit errorOccurred(QStringLiteral("This collection item cannot be removed."));
            return;
        }
        Async::runScoped(
            this, m_api->removeCollectionItems(descriptor.id, { movie.id }),
            [this]() {
                emit operationSucceeded(QStringLiteral("Removed from collection"));
                refreshAfterMutation();
            },
            [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); });
        return;
    }
    emit errorOccurred(QStringLiteral("Open a playlist or collection before removing items."));
}

void LibraryManagementController::movePlaylistItemInCurrent(const MovieItem& movie, int delta)
{
    if (!playlistAllowed())
        return;
    if (!m_browse) {
        emit errorOccurred(QStringLiteral("This playlist entry cannot be moved."));
        return;
    }
    const BrowseDescriptor descriptor = m_browse->descriptor();
    if (descriptor.kind != BrowseKind::Playlist || movie.playlistItemId.isEmpty()) {
        emit errorOccurred(QStringLiteral("This playlist entry cannot be moved."));
        return;
    }

    int currentIndex = -1;
    const std::vector<MovieItem>& items = m_browse->items()->movies();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (items[static_cast<size_t>(i)].playlistItemId == movie.playlistItemId) {
            currentIndex = i;
            break;
        }
    }
    if (currentIndex < 0)
        return;
    const int newIndex = std::clamp(currentIndex + delta, 0, std::max(0, static_cast<int>(items.size()) - 1));
    if (newIndex == currentIndex)
        return;

    Async::runScoped(
        this, m_api->movePlaylistItem(descriptor.id, movie.playlistItemId, newIndex),
        [this]() {
            emit operationSucceeded(QStringLiteral("Playlist item moved"));
            refreshAfterMutation();
        },
        [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); });
}

void LibraryManagementController::renameItem(const MovieItem& movie, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (movie.id.isEmpty() || trimmed.isEmpty()) {
        emit errorOccurred(QStringLiteral("Choose an item and name first."));
        return;
    }
    if (!renameAllowed(movie.itemType))
        return;

    auto onRenamed = [this, itemId = movie.id]() {
        emit operationSucceeded(QStringLiteral("Item renamed"));
        refreshAfterMutation(itemId);
    };
    auto onError = [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); };
    if (movie.itemType == QStringLiteral("Playlist")) {
        Async::runScoped(this, m_api->updatePlaylistName(movie.id, trimmed), onRenamed, onError);
        return;
    }
    Async::runScoped(this, m_api->renameItem(movie.id, trimmed), onRenamed, onError);
}

void LibraryManagementController::deleteItem(const MovieItem& movie)
{
    if (movie.id.isEmpty()) {
        emit errorOccurred(QStringLiteral("Choose an item before deleting."));
        return;
    }
    if (!deleteAllowed())
        return;

    Async::runScoped(
        this, m_api->deleteItem(movie.id),
        [this, itemId = movie.id]() {
            emit operationSucceeded(QStringLiteral("Item deleted"));
            refreshAfterMutation(itemId);
        },
        [this](const std::exception_ptr& error) { emit errorOccurred(exceptionMessage(error)); });
}

} // namespace JellyfinNative

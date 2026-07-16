#include "DiscoveredServerModel.h"

namespace JellyfinNative {

DiscoveredServerModel::DiscoveredServerModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DiscoveredServerModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_servers.size());
}

QVariant DiscoveredServerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const auto& server = m_servers[static_cast<size_t>(index.row())];
    switch (role) {
    case IdRole:
        return server.id;
    case NameRole:
        return server.name;
    case AddressRole:
        return server.address;
    case OnlineRole:
        return m_onlineServers.contains(server.id) || m_onlineServers.contains(server.address);
    default:
        return {};
    }
}

QHash<int, QByteArray> DiscoveredServerModel::roleNames() const
{
    return {
        { IdRole, "serverId" },
        { NameRole, "name" },
        { AddressRole, "address" },
        { OnlineRole, "online" },
    };
}

void DiscoveredServerModel::setServers(const std::vector<DiscoveredServer>& servers, bool online)
{
    beginResetModel();
    m_servers = servers;
    m_onlineServers.clear();
    if (online) {
        for (const DiscoveredServer& server : servers) {
            m_onlineServers.insert(server.id);
            m_onlineServers.insert(server.address);
        }
    }
    endResetModel();
}

void DiscoveredServerModel::upsertServer(const DiscoveredServer& server, bool online)
{
    if (online) {
        m_onlineServers.insert(server.id);
        m_onlineServers.insert(server.address);
    }
    for (int i = 0; i < rowCount(); ++i) {
        auto& existing = m_servers[static_cast<size_t>(i)];
        if (existing.id == server.id || existing.address == server.address) {
            DiscoveredServer updated = server;
            if ((updated.id.isEmpty() || updated.id == updated.address) && !existing.id.isEmpty())
                updated.id = existing.id;
            existing = updated;
            emit dataChanged(index(i, 0), index(i, 0));
            return;
        }
    }

    const int newRow = rowCount();
    beginInsertRows({}, newRow, newRow);
    m_servers.push_back(server);
    endInsertRows();
}

void DiscoveredServerModel::clear()
{
    beginResetModel();
    m_servers.clear();
    m_onlineServers.clear();
    endResetModel();
}

DiscoveredServer DiscoveredServerModel::serverAt(int index) const
{
    if (index < 0 || index >= rowCount())
        return {};
    return m_servers[static_cast<size_t>(index)];
}

std::vector<DiscoveredServer> DiscoveredServerModel::servers() const
{
    return m_servers;
}

} // namespace JellyfinNative

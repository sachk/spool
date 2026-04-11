#pragma once

#include "../common/JellyfinTypes.h"

#include <QAbstractListModel>

#include <vector>

namespace JellyfinNative {

class DiscoveredServerModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        AddressRole,
    };

    explicit DiscoveredServerModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setServers(const std::vector<DiscoveredServer> &servers);
    void upsertServer(const DiscoveredServer &server);
    void clear();
    DiscoveredServer serverAt(int index) const;
    std::vector<DiscoveredServer> servers() const;

private:
    std::vector<DiscoveredServer> m_servers;
};

} // namespace JellyfinNative

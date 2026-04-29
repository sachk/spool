#pragma once

#include "../common/JellyfinTypes.h"

#include <QAbstractListModel>

#include <vector>

namespace JellyfinNative {

class LibraryListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        CollectionTypeRole,
        ImageUrlRole,
        ImageTagRole,
    };

    explicit LibraryListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setLibraries(const std::vector<LibraryItem> &libraries);
    void clear();
    LibraryItem libraryAt(int index) const;

private:
    std::vector<LibraryItem> m_libraries;
};

} // namespace JellyfinNative

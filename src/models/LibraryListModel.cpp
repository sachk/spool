#include "LibraryListModel.h"

namespace JellyfinNative {

LibraryListModel::LibraryListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LibraryListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_libraries.size());
}

QVariant LibraryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const auto &library = m_libraries[static_cast<size_t>(index.row())];
    switch (role) {
    case IdRole:
        return library.id;
    case NameRole:
        return library.name;
    case CollectionTypeRole:
        return library.collectionType;
    case ImageUrlRole:
        return library.imageUrl;
    case ImageTagRole:
        return library.imageTag;
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryListModel::roleNames() const
{
    return {
        {IdRole, "libraryId"},
        {NameRole, "name"},
        {CollectionTypeRole, "collectionType"},
        {ImageUrlRole, "imageUrl"},
        {ImageTagRole, "imageTag"},
    };
}

void LibraryListModel::setLibraries(const std::vector<LibraryItem> &libraries)
{
    beginResetModel();
    m_libraries = libraries;
    endResetModel();
}

void LibraryListModel::clear()
{
    beginResetModel();
    m_libraries.clear();
    endResetModel();
}

LibraryItem LibraryListModel::libraryAt(int index) const
{
    if (index < 0 || index >= rowCount())
        return {};
    return m_libraries[static_cast<size_t>(index)];
}

} // namespace JellyfinNative

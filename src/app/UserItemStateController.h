#pragma once

#include <QObject>
#include <QString>

namespace JellyfinNative {

class ContentModelController;
class CurrentItemsController;
class HomeModelController;
class SearchController;

class UserItemStateController final : public QObject
{
    Q_OBJECT

public:
    UserItemStateController(CurrentItemsController *currentItems,
                            HomeModelController *home,
                            ContentModelController *content,
                            SearchController *search,
                            QObject *parent = nullptr);

    void applyResumeTicks(const QString &itemId, qint64 positionTicks);
    void applyFavorite(const QString &itemId, bool favorite);
    void applyPlayed(const QString &itemId, bool played);

signals:
    void favoriteChanged(const QString &itemId, bool favorite);
    void playedChanged(const QString &itemId, bool played);

private:
    CurrentItemsController *m_currentItems = nullptr;
    HomeModelController *m_home = nullptr;
    ContentModelController *m_content = nullptr;
    SearchController *m_search = nullptr;
};

} // namespace JellyfinNative

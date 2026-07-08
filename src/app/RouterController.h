#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace JellyfinNative {

class RouterController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString route READ route NOTIFY routeChanged)
    Q_PROPERTY(QString previousRoute READ previousRoute NOTIFY routeChanged)
    Q_PROPERTY(QVariantMap args READ args NOTIFY routeChanged)
    Q_PROPERTY(QVariantList stack READ stack NOTIFY stackChanged)
    Q_PROPERTY(bool canPop READ canPop NOTIFY stackChanged)

public:
    explicit RouterController(QObject *parent = nullptr);

    QString route() const;
    QString previousRoute() const;
    QVariantMap args() const;
    QVariantList stack() const;
    bool canPop() const;

    Q_INVOKABLE void reset(const QString& route = QStringLiteral("home"), const QVariantMap& args = {});
    Q_INVOKABLE void push(const QString& route, const QVariantMap& args = {});
    Q_INVOKABLE void replace(const QString& route, const QVariantMap& args = {});
    Q_INVOKABLE bool pop(const QString& fallbackRoute = QStringLiteral("home"));

signals:
    void routeChanged();
    void stackChanged();

private:
    QVariantMap frame(const QString& route, const QVariantMap& args) const;
    void setFrame(const QString& route, const QVariantMap& args, const QString& previousRoute);

    QString m_route = QStringLiteral("login");
    QString m_previousRoute = QStringLiteral("home");
    QVariantMap m_args;
    QVariantList m_stack;
};

} // namespace JellyfinNative

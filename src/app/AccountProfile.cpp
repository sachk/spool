#include "AccountProfile.h"

#include <QCryptographicHash>
#include <QUrl>

namespace JellyfinNative {

QString AccountProfile::displayLabel() const
{
    if (userName.isEmpty())
        return serverName;
    if (serverName.isEmpty())
        return userName;
    return userName + QStringLiteral(" · ") + serverName;
}

QString canonicalServerUrl(const QString& input)
{
    QUrl url = QUrl::fromUserInput(input.trimmed());
    if (!url.isValid() || url.host().isEmpty())
        return {};

    url.setScheme(url.scheme().toLower());
    url.setHost(url.host().toLower());
    if ((url.scheme() == QStringLiteral("http") && url.port() == 80)
        || (url.scheme() == QStringLiteral("https") && url.port() == 443)) {
        url.setPort(-1);
    }
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    url.setPath(path);
    url.setQuery({});
    url.setFragment({});
    return url.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded);
}

QString accountProfileId(const QString& serverId, const QString& serverUrl, const QString& userId)
{
    const QString serverIdentity
        = serverId.trimmed().isEmpty() ? canonicalServerUrl(serverUrl) : serverId.trimmed().toLower();
    const QByteArray identity = serverIdentity.toUtf8() + '\0' + userId.trimmed().toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(24));
}

} // namespace JellyfinNative

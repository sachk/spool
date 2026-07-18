#pragma once

#include <QString>

namespace JellyfinNative {

struct AccountProfile {
    QString profileId;
    QString serverId;
    QString serverName;
    QString serverUrl;
    QString userId;
    QString userName;
    QString accessToken;
    QString avatarTag;
    qint64 lastUsedAt = 0;
    qint64 createdAt = 0;
    bool needsAuthentication = false;

    QString displayLabel() const;
};

QString canonicalServerUrl(const QString& input);
QString accountProfileId(const QString& serverId, const QString& serverUrl, const QString& userId);

} // namespace JellyfinNative

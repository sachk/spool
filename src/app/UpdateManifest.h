#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <optional>

namespace JellyfinNative {

enum class UpdateChannel {
    Release,
    Prerelease,
};

struct AndroidUpdateRelease {
    UpdateChannel channel = UpdateChannel::Release;
    QString version;
    int versionCode = 0;
    QString notes;
    QUrl releaseUrl;
    QUrl apkUrl;
    QByteArray apkSha256;
    qint64 apkSize = 0;
};

struct UpdateManifestResult {
    std::optional<AndroidUpdateRelease> release;
    QString error;
};

UpdateManifestResult selectAndroidUpdate(
    const QByteArray& manifest, int currentVersionCode, bool allowPrerelease, const QString& assetKey);

} // namespace JellyfinNative

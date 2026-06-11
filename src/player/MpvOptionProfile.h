#pragma once

#include <QByteArray>
#include <QString>

#include <vector>

namespace JellyfinNative {

struct MpvOption {
    QByteArray name;
    QByteArray value;
};

class MpvOptionProfile final
{
public:
    enum class Platform {
        Desktop,
        WebOS,
    };

    static std::vector<MpvOption> startupOptions(
        Platform platform, const QString &audioOutputMode,
        const QByteArray &logPath);
};

} // namespace JellyfinNative

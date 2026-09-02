#include "platform/PlatformSettingsPolicy.h"

#include "app/SettingsSchema.h"

#include <QFontDatabase>

namespace JellyfinNative {
namespace {
    constexpr SettingChoice kChoices[]
        = { { "auto", "Automatic" }, { "pipewire", "PipeWire" }, { "pulse", "PulseAudio" }, { "alsa", "ALSA" } };
}

const PlatformAudioOutputPolicy& platformAudioOutputPolicy()
{
    static const PlatformAudioOutputPolicy policy { kChoices, 4, "auto" };
    return policy;
}

QString normalizedPlatformAudioOutputMode(const QString& mode)
{
    return mode == QStringLiteral("pipewire") || mode == QStringLiteral("pulse") || mode == QStringLiteral("alsa")
        ? mode
        : QStringLiteral("auto");
}

QStringList platformSystemSubtitleFonts()
{
    QStringList families = QFontDatabase::families();
    families.sort(Qt::CaseInsensitive);
    families.removeDuplicates();
    return families;
}

int platformDefaultUiScalePercent()
{
    return 100;
}

const char *platformDefaultArtworkFormat()
{
    return "webp";
}

bool platformUsesPerOutputAudioDelay()
{
    return false;
}
bool platformDefaultCastButtonEnabled()
{
    return true;
}
bool platformDefaultRemoteControlTargetEnabled()
{
    return true;
}
QString normalizedPlatformAudioRoute(const QString& output)
{
    return output;
}
QString platformAudioRouteDisplayName(const QString&)
{
    return QStringLiteral("Global");
}
QString platformAudioDelayStorageKey(const QString&)
{
    return QStringLiteral("settings/audioDelayMs");
}
int platformAutomaticAudioDelayMs(const QString&, int, int)
{
    return 0;
}

} // namespace JellyfinNative

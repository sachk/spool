#include "platform/PlatformSettingsPolicy.h"

#include "app/SettingsSchema.h"
#include "platform/webos/WebOSAudioSyncPolicy.h"

namespace JellyfinNative {
namespace {
    constexpr SettingChoice kChoices[] = { { "alsa", "ALSA" }, { "starfish-pcm", "Starfish" } };
}

const PlatformAudioOutputPolicy& platformAudioOutputPolicy()
{
    static const PlatformAudioOutputPolicy policy { kChoices, 2, "alsa" };
    return policy;
}

QString normalizedPlatformAudioOutputMode(const QString& mode)
{
    return mode == QStringLiteral("starfish") || mode == QStringLiteral("starfish-pcm") ? QStringLiteral("starfish-pcm")
                                                                                        : QStringLiteral("alsa");
}

QStringList platformSystemSubtitleFonts()
{
    return {};
}

int platformDefaultUiScalePercent()
{
    return 150;
}
bool platformUsesPerOutputAudioDelay()
{
    return true;
}
bool platformDefaultCastButtonEnabled()
{
    return false;
}
bool platformDefaultRemoteControlTargetEnabled()
{
    return true;
}
QString normalizedPlatformAudioRoute(const QString& output)
{
    return AudioSyncPolicy::normalizedOutputKey(output);
}
QString platformAudioRouteDisplayName(const QString& output)
{
    return AudioSyncPolicy::outputDisplayName(output);
}
QString platformAudioDelayStorageKey(const QString& output)
{
    return AudioSyncPolicy::delayStorageKey(output);
}
int platformAutomaticAudioDelayMs(const QString& output, int displayLatencyMs, int outputLatencyMs)
{
    return AudioSyncPolicy::automaticBaseDelayMs(output, displayLatencyMs, outputLatencyMs);
}

} // namespace JellyfinNative

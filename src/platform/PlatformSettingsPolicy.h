#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace JellyfinNative {

struct SettingChoice;

struct PlatformAudioOutputPolicy {
    const SettingChoice *choices = nullptr;
    qsizetype choiceCount = 0;
    const char *defaultValue = "auto";
};

const PlatformAudioOutputPolicy& platformAudioOutputPolicy();
QString normalizedPlatformAudioOutputMode(const QString& mode);
QStringList platformSystemSubtitleFonts();

int platformDefaultUiScalePercent();
// Which image codec this device should ask the server for. Decoding WebP costs
// roughly three times what an equivalent JPEG costs, measured on a 2018 LG TV,
// and a TV has neither the cores to hide that nor a hardware image decoder to
// take it off the CPU. Everything with a desktop-class CPU spends the cycles
// and takes the smaller download instead.
const char *platformDefaultArtworkFormat();
bool platformUsesPerOutputAudioDelay();
bool platformDefaultCastButtonEnabled();
bool platformDefaultRemoteControlTargetEnabled();
QString normalizedPlatformAudioRoute(const QString& output);
QString platformAudioRouteDisplayName(const QString& output);
QString platformAudioDelayStorageKey(const QString& output);
int platformAutomaticAudioDelayMs(const QString& output, int displayLatencyMs, int outputLatencyMs);

} // namespace JellyfinNative

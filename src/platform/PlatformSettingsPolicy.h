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
bool platformUsesPerOutputAudioDelay();
bool platformDefaultCastButtonEnabled();
bool platformDefaultRemoteControlTargetEnabled();
QString normalizedPlatformAudioRoute(const QString& output);
QString platformAudioRouteDisplayName(const QString& output);
QString platformAudioDelayStorageKey(const QString& output);
int platformAutomaticAudioDelayMs(const QString& output, int displayLatencyMs, int outputLatencyMs);

} // namespace JellyfinNative

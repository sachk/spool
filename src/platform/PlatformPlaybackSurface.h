#pragma once

#include "player/MpvOptionProfile.h"

#include <QString>

#include <functional>

class QObject;
struct mpv_handle;

namespace JellyfinNative {

class NativeAppWindow;
struct PlaybackSession;

bool platformIdleMpvPreparationEnabled();
void runAfterPlatformMpvLoaded(std::function<void()> callback);
MpvOptionProfile::Platform platformMpvOptionProfile();
QString platformPlaybackBackendName();

bool configurePlatformMpvSurface(
    mpv_handle *handle, NativeAppWindow& window, bool needsVideoSurface, QString& errorMessage);
bool attachPlatformMpvSurface(mpv_handle *handle, bool needsVideoSurface, QObject& context,
    std::function<void(const QString&)> errorHandler, QString& errorMessage);
bool releasePlatformMpvSurface();
QString platformPreparingStatus(bool needsVideoSurface);
bool applyPlatformSubtitlePreload(
    mpv_handle *handle, const PlaybackSession& session, const QString& preferredLanguage, QString& errorMessage);

bool platformUsesBackgroundPlaybackPolicy();
void platformAudioTrackChanged(int index);

} // namespace JellyfinNative

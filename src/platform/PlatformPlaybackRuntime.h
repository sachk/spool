#pragma once

#include <QtGlobal>

class QObject;

namespace JellyfinNative {

class JellyfinApiFacade;

qint64 platformAudioDecodeCpuTimeNs();
void configurePlatformPlaybackCapabilities(JellyfinApiFacade& api, QObject& callbackContext);

} // namespace JellyfinNative

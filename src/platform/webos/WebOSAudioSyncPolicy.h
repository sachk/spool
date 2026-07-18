#pragma once

#include <QString>

namespace JellyfinNative::AudioSyncPolicy {

QString normalizedOutputKey(const QString& output);
QString outputDisplayName(const QString& output);
QString delayStorageKey(const QString& output);
int automaticBaseDelayMs(const QString& output, int displayLatencyMs, int outputLatencyMs);

}

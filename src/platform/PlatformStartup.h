#pragma once

#include <QSurfaceFormat>

class QString;

namespace JellyfinNative {

class NativeAppWindow;

bool configurePlatformEnvironment(const QString& appRootPath);
QSurfaceFormat platformSurfaceFormat();
void configurePlatformWindow(NativeAppWindow& window);

} // namespace JellyfinNative

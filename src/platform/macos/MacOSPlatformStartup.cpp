#include "platform/PlatformStartup.h"

#include "platform/NativeAppWindow.h"

#include <QByteArray>

namespace JellyfinNative {

bool configurePlatformEnvironment(const QString&)
{
    if (qEnvironmentVariableIsSet("JELLYFIN_NATIVE_VERBOSE_QT")) {
        qputenv("QT_DEBUG_PLUGINS", QByteArrayLiteral("1"));
        qputenv("QT_LOGGING_RULES",
            QByteArrayLiteral("qt.qml*=true;qt.qpa*=true;qt.scenegraph*=true;qt.quick*=true;qt.plugin*=true"));
    }
    return true;
}

QSurfaceFormat platformSurfaceFormat()
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    // macOS exposes modern OpenGL only through a core profile. A profile-less
    // request can leave Qt with legacy GL, which libplacebo cannot initialize.
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setAlphaBufferSize(8);
    return format;
}

void configurePlatformWindow(NativeAppWindow&) { }

} // namespace JellyfinNative

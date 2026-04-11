#include "api/JellyfinApiFacade.h"
#include "app/AppController.h"
#include "app/NativeAppWindow.h"
#include "app/QmlNetworkAccessManagerFactory.h"
#include "cache/DatabaseManager.h"
#include "discovery/DiscoveryController.h"
#include "player/PlayerController.h"

extern "C" {
#include <limits.h>
#include <signal.h>
#include <unistd.h>

#include <luna-service2/lunaservice.h>
#include <webos-helpers/libhelpers.h>
}

#include <QtPlugin>

Q_IMPORT_PLUGIN(QWaylandIntegrationPlugin)
Q_IMPORT_PLUGIN(QWaylandEglClientBufferPlugin)
Q_IMPORT_PLUGIN(QWaylandWlShellIntegrationPlugin)
Q_IMPORT_PLUGIN(QJpegPlugin)
Q_IMPORT_PLUGIN(QWebpPlugin)
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QGuiApplication>
#include <QList>
#include <QMessageLogContext>
#include <QQmlError>
#include <QQmlContext>
#include <QQmlEngine>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QMetaObject>
#include <QQuickStyle>
#include <QSurfaceFormat>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <memory>

namespace {

constexpr auto kAppId = "com.codex.jellyfinwebosnative";
constexpr auto kAppLogPath = "/tmp/com.codex.jellyfinwebosnative.log";

FILE *g_logFile = nullptr;

bool resolveAppRoot(char *buffer, size_t size)
{
    const ssize_t length = readlink("/proc/self/exe", buffer, size - 1);
    if (length < 0)
        return false;
    buffer[length] = '\0';

    char *lastSlash = strrchr(buffer, '/');
    if (!lastSlash)
        return false;
    *lastSlash = '\0';
    lastSlash = strrchr(buffer, '/');
    if (!lastSlash)
        return false;
    *lastSlash = '\0';
    return true;
}

bool ensureWaylandEnv()
{
    const char *runtimeDir = getenv("XDG_RUNTIME_DIR");
    const char *display = getenv("WAYLAND_DISPLAY");

    if ((!runtimeDir || !runtimeDir[0]) && access("/tmp/xdg", X_OK) == 0) {
        setenv("XDG_RUNTIME_DIR", "/tmp/xdg", 1);
        runtimeDir = "/tmp/xdg";
    }

    if (!display || !display[0]) {
        if (runtimeDir && runtimeDir[0]) {
            char candidate[PATH_MAX];
            if (snprintf(candidate, sizeof(candidate), "%s/wayland-0", runtimeDir) < static_cast<int>(sizeof(candidate)) &&
                access(candidate, F_OK) == 0) {
                setenv("WAYLAND_DISPLAY", "wayland-0", 1);
                display = "wayland-0";
            }
        }
    }

    return runtimeDir && runtimeDir[0] && display && display[0];
}

void handleSignal(int)
{
    QCoreApplication::quit();
}

void logLine(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);

    if (!g_logFile)
        return;

    va_start(ap, fmt);
    vfprintf(g_logFile, fmt, ap);
    fputc('\n', g_logFile);
    fflush(g_logFile);
    va_end(ap);
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const char *level = "debug";
    switch (type) {
    case QtDebugMsg:
        level = "debug";
        break;
    case QtInfoMsg:
        level = "info";
        break;
    case QtWarningMsg:
        level = "warn";
        break;
    case QtCriticalMsg:
        level = "crit";
        break;
    case QtFatalMsg:
        level = "fatal";
        break;
    }

    const QByteArray local = message.toLocal8Bit();
    if (context.category && context.category[0])
        logLine("[qt:%s] %s: %s", level, context.category, local.constData());
    else
        logLine("[qt:%s] %s", level, local.constData());

    if (type == QtFatalMsg)
        abort();
}

void logQmlWarnings(const QList<QQmlError> &warnings)
{
    for (const QQmlError &warning : warnings)
        logLine("[qml] %s", qPrintable(warning.toString()));
}

bool lunaNoopCallback(LSHandle *, LSMessage *message, void *)
{
    if (message && LSMessageGetPayload(message))
        logLine("[ls2] %s", LSMessageGetPayload(message));
    return true;
}

bool lunaLifecycleCallback(LSHandle *, LSMessage *message, void *userData)
{
    auto *window = static_cast<JellyfinNative::NativeAppWindow *>(userData);
    const char *payload = message ? LSMessageGetPayload(message) : nullptr;
    if (!payload)
        return true;

    logLine("[ls2-lifecycle] %s", payload);

    // Parse the event field to detect relaunch
    const QByteArray raw = QByteArray::fromRawData(payload, static_cast<int>(strlen(payload)));
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return true;

    const QString event = doc.object().value(QStringLiteral("event")).toString();
    if (event == QStringLiteral("relaunch")) {
        logLine("[ls2-lifecycle] relaunch detected, bringing window to foreground");
        QMetaObject::invokeMethod(window, [window]() {
            window->showFullScreen();
            window->requestActivate();
            window->raise();
        }, Qt::QueuedConnection);
    }

    return true;
}

} // namespace

int main(int argc, char **argv)
{
    g_logFile = fopen(kAppLogPath, "w");
    setvbuf(stderr, nullptr, _IOLBF, 0);
    if (g_logFile)
        setvbuf(g_logFile, nullptr, _IOLBF, 0);

    logLine("%s starting", kAppId);

    char appRoot[PATH_MAX];
    if (!resolveAppRoot(appRoot, sizeof(appRoot)))
        return 1;

    setenv("APPID", kAppId, 1);
    setenv("DISPLAY_ID", "0", 1);
    setenv("STARFISH_AUDIO_HINT", "1", 1);
    setenv("QT_QPA_PLATFORM", "wayland-egl", 1);
    setenv("QT_WAYLAND_SHELL_INTEGRATION", "wl-shell", 1);
    setenv("QT_WAYLAND_TEXT_INPUT_PROTOCOL", "qt_text_input_method_v1", 1);
    unsetenv("QT_IM_MODULE");
    setenv("QT_QPA_FONTDIR", "/usr/share/fonts", 1);
    setenv("QT_NO_GLIB", "1", 1);
    if (qEnvironmentVariableIsSet("JELLYFIN_NATIVE_VERBOSE_QT")) {
        setenv("QT_DEBUG_PLUGINS", "1", 1);
        setenv("QT_LOGGING_RULES",
               "qt.qml*=true;qt.qpa*=true;qt.scenegraph*=true;qt.quick*=true;qt.plugin*=true",
               1);
    } else {
        unsetenv("QT_DEBUG_PLUGINS");
        unsetenv("QT_LOGGING_RULES");
    }

    const QString appRootPath = QString::fromUtf8(appRoot);
    setenv("QT_PLUGIN_PATH", QFile::encodeName(appRootPath + "/qt-plugins").constData(), 1);
    setenv("QT_QPA_PLATFORM_PLUGIN_PATH", QFile::encodeName(appRootPath + "/qt-plugins/platforms").constData(), 1);
    setenv("QML2_IMPORT_PATH", QFile::encodeName(appRootPath + "/qt-qml").constData(), 1);

    if (!ensureWaylandEnv())
        return 1;

    logLine("app root: %s", appRoot);
    logLine("QT_QPA_PLATFORM=%s", qgetenv("QT_QPA_PLATFORM").constData());
    logLine("QT_PLUGIN_PATH=%s", qgetenv("QT_PLUGIN_PATH").constData());
    logLine("QML2_IMPORT_PATH=%s", qgetenv("QML2_IMPORT_PATH").constData());
    logLine("QT_WAYLAND_TEXT_INPUT_PROTOCOL=%s", qgetenv("QT_WAYLAND_TEXT_INPUT_PROTOCOL").constData());
    logLine("QT_IM_MODULE=%s", qgetenv("QT_IM_MODULE").constData());

    qInstallMessageHandler(qtMessageHandler);
    QLoggingCategory::setFilterRules(QStringLiteral("qt.*.debug=false\nqt.*.info=false"));

    struct sigaction action = {};
    action.sa_handler = handleSignal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setMajorVersion(2);
    format.setMinorVersion(0);
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Jellyfin Native"));
    app.setOrganizationName(QStringLiteral("Codex"));
    app.setApplicationDisplayName(QStringLiteral("Jellyfin Native"));

    auto *networkAccessManager = new QNetworkAccessManager();
    auto *diskCache = new QNetworkDiskCache(networkAccessManager);
    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cachePath);
    diskCache->setCacheDirectory(cachePath + QStringLiteral("/network-cache"));
    diskCache->setMaximumCacheSize(256LL * 1024LL * 1024LL);
    networkAccessManager->setCache(diskCache);

    JellyfinNative::DatabaseManager database;
    if (!database.initialize(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/cache.sqlite")))
        return 1;

    auto discovery = std::make_unique<JellyfinNative::DiscoveryController>();
    auto api = std::make_unique<JellyfinNative::JellyfinApiFacade>(networkAccessManager);
    api->setDeviceIdentity(QUuid::createUuid().toString(QUuid::WithoutBraces),
                           QStringLiteral("LG webOS TV"),
                           QStringLiteral("0.1.0"));

    JellyfinNative::NativeAppWindow window(QString::fromLatin1(kAppId));
    if (!window.prepareForUiSurface()) {
        logLine("failed to initialize Qt webOS UI surface");
        return 1;
    }

    auto player = std::make_unique<JellyfinNative::PlayerController>(&window, api.get());
    auto controller =
        std::make_unique<JellyfinNative::AppController>(&database, discovery.get(), api.get(), player.get());

    auto *qmlNetworkFactory = new JellyfinNative::QmlNetworkAccessManagerFactory(
        cachePath + QStringLiteral("/qml-image-cache"), 512LL * 1024LL * 1024LL);
    window.engine()->setNetworkAccessManagerFactory(qmlNetworkFactory);
    window.engine()->addImportPath(appRootPath + QStringLiteral("/qt-qml"));
    QObject::connect(window.engine(), &QQmlEngine::warnings, &logQmlWarnings);
    QObject::connect(&window, &QQuickView::statusChanged, [](QQuickView::Status status) {
        logLine("view status changed: %d", static_cast<int>(status));
    });
    window.rootContext()->setContextProperty(QStringLiteral("appController"), controller.get());
    window.setSource(QUrl(QStringLiteral("qrc:/qt/qml/JellyfinWebOS/qml/Main.qml")));
    if (window.status() == QQuickView::Error) {
        logQmlWarnings(window.errors());
        return 1;
    }
    window.showFullScreen();
    window.requestActivate();

    std::unique_ptr<HContext> lunaContext(new HContext());
    lunaContext->pub = true;
    lunaContext->multiple = true;
    lunaContext->callback = &lunaLifecycleCallback;
    lunaContext->userdata = &window;
    if (HLunaServiceCall("luna://com.webos.service.applicationmanager/registerApp", "{}", lunaContext.get())) {
        logLine("HLunaServiceCall registerApp failed (non-fatal)");
        lunaContext.reset();
    } else {
        logLine("HLunaServiceCall registerApp OK");
    }

    std::unique_ptr<HContext> imeContext(new HContext());
    imeContext->pub = true;
    imeContext->multiple = true;
    imeContext->callback = &lunaNoopCallback;
    imeContext->userdata = nullptr;
    if (HLunaServiceCall("luna://com.webos.service.ime/registerRemoteKeyboard", "{\"subscribe\":true}", imeContext.get())) {
        logLine("HLunaServiceCall registerRemoteKeyboard failed (non-fatal)");
        imeContext.reset();
    } else {
        logLine("HLunaServiceCall registerRemoteKeyboard OK");
    }

    QTimer::singleShot(0, &window, [&window]() {
        window.showFullScreen();
        window.requestActivate();
    });
    QTimer::singleShot(300, &window, [&window]() {
        window.showFullScreen();
        window.requestActivate();
    });
    QTimer::singleShot(0, controller.get(), &JellyfinNative::AppController::initialize);

    return app.exec();
}

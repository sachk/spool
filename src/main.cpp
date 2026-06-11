#include "api/JellyfinApiFacade.h"
#include "app/AppController.h"
#include "app/LocalizationManager.h"
#include "app/NativeAppWindow.h"
#include "app/QmlNetworkAccessManagerFactory.h"
#include "cache/DatabaseManager.h"
#include "diagnostics/Diagnostics.h"
#include "discovery/DiscoveryController.h"
#include "player/PlayerController.h"

extern "C" {
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>

#ifdef JELLYFIN_NATIVE_WEBOS
#include <luna-service2/lunaservice.h>
#include <webos-helpers/libhelpers.h>
#endif
}

#ifdef JELLYFIN_NATIVE_WEBOS
#include <QtPlugin>

Q_IMPORT_PLUGIN(QWaylandIntegrationPlugin)
Q_IMPORT_PLUGIN(QWaylandEglClientBufferPlugin)
Q_IMPORT_PLUGIN(QWaylandWlShellIntegrationPlugin)
Q_IMPORT_PLUGIN(QJpegPlugin)
Q_IMPORT_PLUGIN(QWebpPlugin)
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
Q_IMPORT_PLUGIN(QVirtualKeyboardPlugin)
#endif

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
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QMetaObject>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSocketNotifier>
#include <QSurfaceFormat>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <clocale>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {

constexpr auto kAppId = "com.sachk.tern";
constexpr auto kAppVersion = JELLYFIN_VERSION;
#ifdef JELLYFIN_NATIVE_WEBOS
constexpr auto kAppLogPath = "/tmp/com.sachk.tern.log";
constexpr qint64 kNetworkDiskCacheBytes = 96LL * 1024LL * 1024LL;
constexpr qint64 kQmlImageDiskCacheBytes = 160LL * 1024LL * 1024LL;
#else
constexpr auto kAppLogPath = "/tmp/com.codex.jellyfinnative-linux.log";
constexpr qint64 kNetworkDiskCacheBytes = 256LL * 1024LL * 1024LL;
constexpr qint64 kQmlImageDiskCacheBytes = 256LL * 1024LL * 1024LL;
#endif

FILE *g_logFile = nullptr;

#ifdef JELLYFIN_NATIVE_WEBOS
// Window pointer captured for the LS2 lifecycle callback, which runs on
// the LS2 dispatch thread. We marshal events onto the GUI thread via
// QMetaObject::invokeMethod against this object.
JellyfinNative::NativeAppWindow *g_lifecycleWindow = nullptr;
#endif

void rotateLogFile(const char *path)
{
    char older[PATH_MAX];
    char newer[PATH_MAX];
    if (snprintf(older, sizeof(older), "%s.2", path) < static_cast<int>(sizeof(older)))
        unlink(older);
    if (snprintf(older, sizeof(older), "%s.1", path) < static_cast<int>(sizeof(older)) &&
        snprintf(newer, sizeof(newer), "%s.2", path) < static_cast<int>(sizeof(newer)))
        rename(older, newer);
    if (snprintf(newer, sizeof(newer), "%s.1", path) < static_cast<int>(sizeof(newer)))
        rename(path, newer);
}

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

#ifdef JELLYFIN_NATIVE_WEBOS
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
#endif

// Self-pipe used to deliver SIGINT/SIGTERM into the Qt event loop. Calling
// QCoreApplication::quit() from a signal handler is async-unsafe — under
// active playback the main thread can be deep in QML/scene-graph code and
// the deferred posted event never gets a chance to run, so Ctrl+C is silently
// ignored. The signal handler instead writes a single byte to the pipe; a
// QSocketNotifier on the main thread reads it and calls quit() safely.
int g_signalPipe[2] = {-1, -1};

void handleSignal(int signalNumber)
{
    const char byte = static_cast<char>(signalNumber > 0 ? signalNumber : 1);
    // write() is async-signal-safe; ignore the result — if the pipe is full
    // the previous byte already armed the notifier.
    ssize_t n = write(g_signalPipe[1], &byte, 1);
    (void)n;
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

#ifdef JELLYFIN_NATIVE_WEBOS
bool lunaNoopCallback(LSHandle *, LSMessage *message, void *)
{
    if (message && LSMessageGetPayload(message))
        logLine("[ls2] %s", LSMessageGetPayload(message));
    return true;
}

// Lifecycle callback for com.webos.service.applicationmanager/registerApp.
// Runs on the LS2 dispatcher thread, so all work must be queued onto the
// GUI thread. We only care about two events:
//   - "relaunch": user picked our icon on the home screen while we were
//     already running. Re-issue webos_shell SetFullScreen so the LSM
//     actually brings us to the foreground (Qt's requestActivate/raise
//     go through wl-shell and are silently ignored by the LSM).
//   - "close":   user closed us from Recent Apps. Quit cleanly via
//     QCoreApplication::quit so the aboutToQuit chain (controller
//     shutdown, mpv teardown, QML source clear) runs normally.
bool lunaLifecycleCallback(LSHandle *, LSMessage *message, void *)
{
    const char *payload = message ? LSMessageGetPayload(message) : nullptr;
    if (!payload)
        return true;

    logLine("[ls2-lifecycle] %s", payload);

    const QByteArray raw = QByteArray::fromRawData(payload, static_cast<int>(strlen(payload)));
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return true;

    const QString event = doc.object().value(QStringLiteral("event")).toString();
    if (event == QStringLiteral("relaunch")) {
        if (g_lifecycleWindow) {
            JellyfinNative::NativeAppWindow *window = g_lifecycleWindow;
            QMetaObject::invokeMethod(window, [window]() {
                logLine("[ls2-lifecycle] relaunch -> bringToFront");
                window->bringToFront();
            }, Qt::QueuedConnection);
        }
    } else if (event == QStringLiteral("close")) {
        const QString reason = doc.object().value(QStringLiteral("reason")).toString();
        if (reason == QStringLiteral("memoryReclaim")) {
            logLine("[ls2-lifecycle] memoryReclaim close ignored");
            return true;
        }
        QMetaObject::invokeMethod(qApp, []() {
            logLine("[ls2-lifecycle] close -> QCoreApplication::quit");
            QCoreApplication::quit();
        }, Qt::QueuedConnection);
    }

    return true;
}
#endif

} // namespace

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Jellyfin Native %s\n", kAppVersion);
            return 0;
        }
    }

    QElapsedTimer startupTimer;
    startupTimer.start();
    rotateLogFile(kAppLogPath);
    g_logFile = fopen(kAppLogPath, "w");
    setvbuf(stderr, nullptr, _IOLBF, 0);
    if (g_logFile)
        setvbuf(g_logFile, nullptr, _IOLBF, 0);

    logLine("%s starting", kAppId);

    // libmpv parses option strings (and many internal numeric values) with the
    // C locale assumption — under any other LC_NUMERIC playback fails to start
    // because option parsing rejects floating-point arguments. Force LC_NUMERIC
    // to C for both this process and any inherited child env so the user does
    // not have to set LC_NUMERIC=C themselves.
    setlocale(LC_NUMERIC, "C");
    setenv("LC_NUMERIC", "C", 1);
    if (setlocale(LC_CTYPE, "C.UTF-8")) {
        setenv("LANG", "C.UTF-8", 1);
        setenv("LC_CTYPE", "C.UTF-8", 1);
    } else if (setlocale(LC_CTYPE, "en_US.UTF-8")) {
        setenv("LANG", "en_US.UTF-8", 1);
        setenv("LC_CTYPE", "en_US.UTF-8", 1);
    }

    char appRoot[PATH_MAX];
    if (!resolveAppRoot(appRoot, sizeof(appRoot)))
        return 1;

    const QString appRootPath = QString::fromUtf8(appRoot);

#ifdef JELLYFIN_NATIVE_WEBOS
    setenv("APPID", kAppId, 1);
    setenv("DISPLAY_ID", "0", 1);
    setenv("STARFISH_AUDIO_HINT", "0", 1);
    setenv("QT_QPA_PLATFORM", "wayland-egl", 1);
    setenv("QSG_RHI_BACKEND", "opengl", 1);
    setenv("QT_WAYLAND_SHELL_INTEGRATION", "wl-shell", 1);
    setenv("QT_WAYLAND_TEXT_INPUT_PROTOCOL", "qt_text_input_method_v1", 1);
    // Use Qt's in-process virtual keyboard as the input method. The webOS
    // LSM does not reliably hand back text from its native IME for
    // arbitrary native apps, and the wayland compositor's text-input
    // protocol path is fragile across webOS versions. QtVirtualKeyboard
    // renders entirely inside our QML scene and handles D-pad navigation
    // between keys natively (FEATURE_vkb_arrow_keynavigation), which is
    // what we want for the magic remote.
    setenv("QT_IM_MODULE", "qtvirtualkeyboard", 1);
    setenv("QT_QPA_FONTDIR", "/usr/share/fonts", 1);
    setenv("QT_NO_GLIB", "1", 1);
    // Suppress all client-side wl_pointer.set_cursor calls so the webOS
    // LSM keeps drawing the magic remote pointer. Honoured by our local
    // qtwayland patch (qtwayland-6.11.0-webos-no-cursor-set.patch) which
    // makes Qt's updateCursor return early. xbmc achieves the same thing
    // by overriding CSeatWebOS::SetCursor to a no-op in its custom QPA.
    setenv("JELLYFIN_QT_NO_CURSOR_SURFACE", "1", 1);
    if (qEnvironmentVariableIsSet("JELLYFIN_NATIVE_VERBOSE_QT")) {
        setenv("QT_DEBUG_PLUGINS", "1", 1);
        setenv("QT_LOGGING_RULES",
               "qt.qml*=true;qt.qpa*=true;qt.scenegraph*=true;qt.quick*=true;qt.plugin*=true",
               1);
    } else {
        unsetenv("QT_DEBUG_PLUGINS");
        unsetenv("QT_LOGGING_RULES");
    }

    setenv("QT_PLUGIN_PATH", QFile::encodeName(appRootPath + "/qt-plugins").constData(), 1);
    setenv("QT_QPA_PLATFORM_PLUGIN_PATH", QFile::encodeName(appRootPath + "/qt-plugins/platforms").constData(), 1);
    setenv("QML2_IMPORT_PATH", QFile::encodeName(appRootPath + "/qt-qml").constData(), 1);

    if (!ensureWaylandEnv())
        return 1;
#else
#ifdef __linux__
    // Default to wayland on Linux desktop; the user can override via the env.
    // Don't touch QT_QPA_PLATFORM on macOS — Qt picks "cocoa" automatically
    // and forcing "wayland" makes the platform-plugin loader fail to start.
    qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM")
                                  ? qgetenv("QT_QPA_PLATFORM")
                                  : QByteArrayLiteral("wayland"));
#endif
    if (qEnvironmentVariableIsSet("JELLYFIN_NATIVE_VERBOSE_QT")) {
        setenv("QT_DEBUG_PLUGINS", "1", 1);
        setenv("QT_LOGGING_RULES",
               "qt.qml*=true;qt.qpa*=true;qt.scenegraph*=true;qt.quick*=true;qt.plugin*=true",
               1);
    }
#endif

    logLine("app root: %s", appRoot);
    logLine("QT_QPA_PLATFORM=%s", qgetenv("QT_QPA_PLATFORM").constData());
    logLine("QT_PLUGIN_PATH=%s", qgetenv("QT_PLUGIN_PATH").constData());
    logLine("QML2_IMPORT_PATH=%s", qgetenv("QML2_IMPORT_PATH").constData());
#ifdef JELLYFIN_NATIVE_WEBOS
    logLine("QT_WAYLAND_TEXT_INPUT_PROTOCOL=%s", qgetenv("QT_WAYLAND_TEXT_INPUT_PROTOCOL").constData());
    logLine("QT_IM_MODULE=%s", qgetenv("QT_IM_MODULE").constData());
#endif

    qInstallMessageHandler(qtMessageHandler);
    QLoggingCategory::setFilterRules(QStringLiteral("qt.*.debug=false\nqt.*.info=false"));

    if (pipe(g_signalPipe) == 0) {
        // Non-blocking write so the signal handler never stalls; reads on the
        // notifier side are also non-blocking via QSocketNotifier semantics.
        for (int fd : {g_signalPipe[0], g_signalPipe[1]}) {
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags >= 0)
                fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    struct sigaction action = {};
    action.sa_handler = handleSignal;
    sigemptyset(&action.sa_mask);
    // SA_RESTART so handler doesn't break long-running syscalls in worker
    // threads; the self-pipe wakes the event loop regardless.
    action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // MpvVideoItem renders into an FBO via libmpv's OpenGL render API; force
    // Qt Quick to use the OpenGL RHI backend (Qt 6 defaults to Vulkan/Metal
    // on some platforms).
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QSurfaceFormat format;
#ifdef JELLYFIN_NATIVE_WEBOS
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setMajorVersion(2);
    format.setMinorVersion(0);
#else
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setMajorVersion(3);
    format.setMinorVersion(3);
#endif
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    logLine("startup: constructing QGuiApplication");
    QGuiApplication app(argc, argv);
    logLine("startup: QGuiApplication constructed");
    app.setApplicationName(QStringLiteral("Jellyfin Native"));
    app.setApplicationVersion(QString::fromLatin1(kAppVersion));
    app.setOrganizationName(QStringLiteral("Codex"));
    app.setApplicationDisplayName(QStringLiteral("Jellyfin Native"));

    const QString diagnosticsRoot = qEnvironmentVariableIsSet("JELLYFIN_DIAGNOSTICS_DIR")
                                        ? QString::fromLocal8Bit(qgetenv("JELLYFIN_DIAGNOSTICS_DIR"))
                                        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/diagnostics");
    JellyfinNative::Diagnostics::initialize(QString::fromLatin1(kAppId), diagnosticsRoot);
    JellyfinNative::Diagnostics::ThreadScope guiThread(QStringLiteral("gui"));
    JellyfinNative::Diagnostics::EventLoopWatchdog eventLoopWatchdog(&app);

    const QStringList arguments = app.arguments();
    if (arguments.contains(QStringLiteral("--diagnose-and-exit")) ||
        arguments.contains(QStringLiteral("--dump-diagnostics"))) {
        JellyfinNative::Diagnostics::dumpDiagnostics(QStringLiteral("command-line"));
        JellyfinNative::Diagnostics::shutdown();
        return 0;
    }
    if (qEnvironmentVariableIntValue("JELLYFIN_DIAGNOSTICS_BLOCK_GUI_MS") > 0) {
        const int blockMs = qEnvironmentVariableIntValue("JELLYFIN_DIAGNOSTICS_BLOCK_GUI_MS");
        QTimer::singleShot(1000, &app, [blockMs]() {
            JellyfinNative::Diagnostics::logEvent(QStringLiteral("simulation"), QStringLiteral("block_gui_begin"), {{QStringLiteral("durationMs"), blockMs}});
            QThread::msleep(static_cast<unsigned long>(blockMs));
            JellyfinNative::Diagnostics::logEvent(QStringLiteral("simulation"), QStringLiteral("block_gui_end"));
        });
    }

    std::unique_ptr<QSocketNotifier> signalNotifier;
    if (g_signalPipe[0] >= 0) {
        signalNotifier = std::make_unique<QSocketNotifier>(
            g_signalPipe[0], QSocketNotifier::Read);
        QObject::connect(signalNotifier.get(), &QSocketNotifier::activated,
                         &app, [&app](QSocketDescriptor, QSocketNotifier::Type) {
            char buf[16];
            ssize_t count = 0;
            while ((count = read(g_signalPipe[0], buf, sizeof(buf))) > 0) {
                for (ssize_t i = 0; i < count; ++i)
                    JellyfinNative::Diagnostics::noteSignal(static_cast<int>(buf[i]));
            }
            logLine("signal received, quitting");
            app.quit();
        });
    }

    auto *networkAccessManager = new QNetworkAccessManager(&app);
    auto *diskCache = new QNetworkDiskCache(networkAccessManager);
    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cachePath);
    diskCache->setCacheDirectory(cachePath + QStringLiteral("/network-cache"));
    diskCache->setMaximumCacheSize(kNetworkDiskCacheBytes);
    networkAccessManager->setCache(diskCache);

    JellyfinNative::DatabaseManager database;
    {
    JellyfinNative::Diagnostics::Phase phase(QStringLiteral("startup"), QStringLiteral("database_initialize"));
    if (!database.initialize(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/cache.sqlite")))
        return 1;
    }

    QString deviceId = database.loadDeviceId();
    if (deviceId.isEmpty()) {
        deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        database.saveDeviceId(deviceId);
    }

    auto discovery = std::make_unique<JellyfinNative::DiscoveryController>();
    auto api = std::make_unique<JellyfinNative::JellyfinApiFacade>(networkAccessManager);
    api->setDeviceIdentity(deviceId,
#ifdef JELLYFIN_NATIVE_WEBOS
                           QStringLiteral("LG webOS TV"),
#else
                           QStringLiteral("Linux Wayland"),
#endif
                           QString::fromLatin1(kAppVersion));

    JellyfinNative::NativeAppWindow window(QString::fromLatin1(kAppId));
    {
    JellyfinNative::Diagnostics::Phase phase(QStringLiteral("startup"), QStringLiteral("prepare_ui_surface"));
    if (!window.prepareForUiSurface()) {
        logLine("failed to initialize Qt webOS UI surface");
        return 1;
    }
    }
    logLine("startup: prepareForUiSurface completed in %lld ms",
            static_cast<long long>(startupTimer.elapsed()));

    auto player = std::make_unique<JellyfinNative::PlayerController>(&window, api.get());
    auto controller =
        std::make_unique<JellyfinNative::AppController>(&database, discovery.get(), api.get(), player.get());

#ifdef JELLYFIN_NATIVE_WEBOS
    // Log state transitions for diagnosis but DO NOT auto-quit when the
    // window is Hidden/Suspended. webOS briefly flips the application
    // state during home-key transitions and launch handoff; killing the
    // process here used to cause an infinite home-relaunch loop where
    // the LSM kept relaunching us only for us to exit again before any
    // surface ever made it to the foreground.
    //
    // We do pause libmpv when the app goes background while playback is
    // active — the decode/audio pipeline keeps running otherwise and the
    // user comes back to find playback drifted. pauseForBackground() is
    // a no-op if the player isn't visible or is already paused.
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &app,
                     [player = player.get()](Qt::ApplicationState state) {
        logLine("application state changed: %d", static_cast<int>(state));
        if (state == Qt::ApplicationHidden || state == Qt::ApplicationSuspended) {
            if (player)
                player->pauseForBackground();
        } else if (state == Qt::ApplicationInactive) {
            if (player)
                player->prepareForBackground();
        } else if (state == Qt::ApplicationActive) {
            if (player)
                player->resyncForForeground();
        }
    });
#endif

    // Shutdown sequence (runs while the event loop and scene graph are still
    // alive, before any of the unique_ptrs below get destructed):
    //   1. Tear mpv down — stops audio/decode threads and frees the render
    //      context. mpv_terminate_destroy joins everything synchronously.
    //   2. Clear the QQuickView's source so QML items unbind from the
    //      `appController` / `nativeWindow` context properties before those
    //      objects are destroyed. Otherwise the bindings keep evaluating
    //      against null pointers and emit a flood of "Cannot read property
    //      'X' of null" warnings during the unwind.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [controller = controller.get(), &window]() {
        JellyfinNative::Diagnostics::setInstanceState(QStringLiteral("shutting_down"));
        JellyfinNative::Diagnostics::Phase shutdownPhase(QStringLiteral("shutdown"), QStringLiteral("aboutToQuit"));
        logLine("aboutToQuit: stopping controllers");
        if (qEnvironmentVariableIntValue("JELLYFIN_DIAGNOSTICS_SHUTDOWN_HANG_MS") > 0) {
            const int hangMs = qEnvironmentVariableIntValue("JELLYFIN_DIAGNOSTICS_SHUTDOWN_HANG_MS");
            JellyfinNative::Diagnostics::logEvent(QStringLiteral("simulation"), QStringLiteral("shutdown_hang_begin"), {{QStringLiteral("durationMs"), hangMs}});
            QThread::msleep(static_cast<unsigned long>(hangMs));
            JellyfinNative::Diagnostics::logEvent(QStringLiteral("simulation"), QStringLiteral("shutdown_hang_end"));
        }
        {
            JellyfinNative::Diagnostics::Phase phase(QStringLiteral("shutdown"), QStringLiteral("controller_shutdown"));
            controller->shutdown();
        }
        logLine("aboutToQuit: clearing QML source");
        {
            JellyfinNative::Diagnostics::Phase phase(QStringLiteral("shutdown"), QStringLiteral("clear_qml_source"));
            window.setSource(QUrl());
        }
        logLine("aboutToQuit: QML source cleared");
    });

    auto *qmlNetworkFactory = new JellyfinNative::QmlNetworkAccessManagerFactory(
        cachePath + QStringLiteral("/qml-image-cache"), kQmlImageDiskCacheBytes);
    window.engine()->setNetworkAccessManagerFactory(qmlNetworkFactory);
    window.engine()->addImageProvider(QStringLiteral("mpv-overlay"),
                                      window.createOverlayImageProvider());
    window.engine()->addImportPath(appRootPath + QStringLiteral("/qt-qml"));
    QObject::connect(window.engine(), &QQmlEngine::warnings, &logQmlWarnings);
    QObject::connect(&window, &QQuickView::statusChanged, [](QQuickView::Status status) {
        logLine("view status changed: %d", static_cast<int>(status));
    });
    auto localization = std::make_unique<JellyfinNative::LocalizationManager>();
    localization->attachToEngine(window.engine());
    if (api) {
        api->setAcceptLanguage(localization->bcp47Locale());
        QObject::connect(localization.get(), &JellyfinNative::LocalizationManager::localeChanged,
                         api.get(), [api = api.get(), loc = localization.get()]() {
            api->setAcceptLanguage(loc->bcp47Locale());
        });
    }
    window.rootContext()->setContextProperty(QStringLiteral("appController"), controller.get());
    window.rootContext()->setContextProperty(QStringLiteral("nativeWindow"), &window);
    window.rootContext()->setContextProperty(QStringLiteral("i18n"), localization.get());
    {
    JellyfinNative::Diagnostics::Phase phase(QStringLiteral("startup"), QStringLiteral("load_qml"));
    // Load through the module registry (not a raw qrc: URL) so the engine uses
    // the qmlcachegen AOT units instead of recompiling every QML file at runtime
    // (which retained ~47MB of QtQml compiler buffers on the private heap and
    // pushed us into webOS memoryReclaim).
    window.loadFromModule("JellyfinWebOS", "Main");
    }
    if (window.status() == QQuickView::Error) {
        logQmlWarnings(window.errors());
        return 1;
    }
    logLine("startup: QML source loaded in %lld ms",
            static_cast<long long>(startupTimer.elapsed()));
#ifdef JELLYFIN_NATIVE_WEBOS
    window.showFullScreen();
#else
    window.show();
#endif
    window.requestActivate();

#ifdef JELLYFIN_NATIVE_WEBOS
    g_lifecycleWindow = &window;

    std::unique_ptr<HContext> lunaContext(new HContext());
    lunaContext->pub = true;
    lunaContext->multiple = true;
    lunaContext->callback = &lunaLifecycleCallback;
    lunaContext->userdata = nullptr;
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
#endif

    QTimer::singleShot(0, controller.get(), &JellyfinNative::AppController::initialize);
    QTimer::singleShot(0, &window, [&window, &startupTimer]() {
        logLine("startup: event loop entered, first-frame path at %lld ms",
                static_cast<long long>(startupTimer.elapsed()));
        JellyfinNative::Diagnostics::setInstanceState(QStringLiteral("running"));
#ifdef JELLYFIN_NATIVE_WEBOS
        window.bringToFront();
#else
        window.show();
        window.requestActivate();
#endif
    });

    const int exitCode = app.exec();
    logLine("app.exec returned: %d", exitCode);
    JellyfinNative::Diagnostics::setInstanceState(QStringLiteral("app_exec_returned"), {{QStringLiteral("exitCode"), exitCode}});
    JellyfinNative::Diagnostics::shutdown();
    return exitCode;
}

#include "api/JellyfinApiFacade.h"
#include "app/AppController.h"
#include "app/ArtworkImageProvider.h"
#include "app/CpuTopology.h"
#include "app/LocalizationManager.h"
#include "app/MemoryBudget.h"
#include "app/RouterController.h"
#include "app/SessionController.h"
#include "app/UserItemStateController.h"
#include "cache/DatabaseManager.h"
#include "common/JellyfinTypes.h"
#include "common/LogRotation.h"
#include "common/TlsTrust.h"
#include "diagnostics/Diagnostics.h"
#include "diagnostics/InputLatencyMonitor.h"
#include "diagnostics/SystemPerformanceMonitor.h"
#include "discovery/DiscoveryController.h"
#include "platform/NativeAppWindow.h"
#include "platform/PlatformApplicationServices.h"
#include "platform/PlatformCapabilities.h"
#include "platform/PlatformPaths.h"
#include "platform/PlatformPlaybackRuntime.h"
#include "platform/PlatformProcess.h"
#include "platform/PlatformStartup.h"
#include "platform/ScreenSaverInhibitor.h"
#include "player/MpvVideoItem.h"
#include "player/PlayerController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QLoggingCategory>
#include <QMessageLogContext>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQmlIncubationController>
#include <QQmlPropertyMap>
#include <QQuickGraphicsConfiguration>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QThread>
#include <QTimer>
#include <qqml.h>

#include <atomic>
#include <clocale>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace {

// The window's stock incubation controller advances async QML construction
// ~5 ms per frame, so a page whose creation costs ~200 ms of CPU takes
// 500-700 ms of wall time to instantiate. A timer-driven controller with a
// larger slice cuts cold page construction and idle prewarming to roughly the
// CPU cost, at worst lengthening frames by the slice while incubating.
class BoostedIncubationController final : public QObject, public QQmlIncubationController {
public:
    explicit BoostedIncubationController(QObject *parent)
        : QObject(parent)
    {
        m_timer.setInterval(16);
        m_timer.setTimerType(Qt::PreciseTimer);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] { incubateFor(kSliceMs); });
    }

protected:
    void incubatingObjectCountChanged(int count) override
    {
        if (count > 0)
            m_timer.start();
        else
            m_timer.stop();
    }

private:
    static constexpr int kSliceMs = 12;
    QTimer m_timer;
};

constexpr auto kAppId = "com.sachk.spool";
constexpr auto kAppVersion = JELLYFIN_VERSION;

FILE *g_logFile = nullptr;
QByteArray g_logPath;
QElapsedTimer g_startupTimer;
std::mutex g_logMutex;

FILE *openRotatedLogFile(const QByteArray& path)
{
    JellyfinNative::rotateLogFile(path.constData());
    return fopen(path.constData(), "w");
}

FILE *openAppLogFile(const QString& appRootPath)
{
    const QByteArray fileName = QFile::encodeName(JellyfinNative::appLogFileName());
    for (const QString& directory : JellyfinNative::appLogDirectories(appRootPath)) {
        if (directory.isEmpty())
            continue;
        QDir().mkpath(directory);
        QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        const QByteArray encodedDirectory = QFile::encodeName(directory);
        const QByteArray path = QFile::encodeName(QDir(directory).filePath(QString::fromUtf8(fileName)));
        if (FILE *file = openRotatedLogFile(path)) {
            QFile::setPermissions(QString::fromLocal8Bit(path), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            g_logPath = path;
            qputenv("JELLYFIN_NATIVE_LOG_DIR", encodedDirectory);
            return file;
        }
    }
    return nullptr;
}

void configurePersistentStartupCaches(const QString& cacheRoot)
{
    const QString fontconfigCache = QDir(cacheRoot).filePath(QStringLiteral("fontconfig"));
    const QString qtShaderCache = QDir(cacheRoot).filePath(QStringLiteral("qtshadercache"));
    QDir().mkpath(fontconfigCache);
    QDir().mkpath(qtShaderCache);

    // These must be in the environment before QGuiApplication is constructed:
    // Qt and fontconfig both snapshot cache locations during platform/font
    // setup. Point them at app-owned persistent storage; never clear them.
    qputenv("XDG_CACHE_HOME", QFile::encodeName(cacheRoot));
    qputenv("FONTCONFIG_CACHE", QFile::encodeName(fontconfigCache));
    qputenv("QT_SHADER_CACHE_PATH", QFile::encodeName(qtShaderCache));
}

void configurePersistentRhiPipelineCache(QQuickWindow& window, const QString& cacheRoot)
{
    const QString rhiCacheDir = QDir(cacheRoot).filePath(QStringLiteral("rhi-pipeline-cache"));
    QDir().mkpath(rhiCacheDir);

    const QString cacheFile = QDir(rhiCacheDir).filePath(QStringLiteral("qt-rhi-pipeline-cache.bin"));
    QQuickGraphicsConfiguration graphicsConfig;
    graphicsConfig.setAutomaticPipelineCache(true);
    graphicsConfig.setPipelineCacheLoadFile(cacheFile);
    graphicsConfig.setPipelineCacheSaveFile(cacheFile);
    window.setGraphicsConfiguration(graphicsConfig);
}

void logLine(const char *fmt, ...)
{
    const std::lock_guard lock(g_logMutex);
    const long long elapsedMs = g_startupTimer.isValid() ? static_cast<long long>(g_startupTimer.elapsed()) : 0;

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%7lld ms] ", elapsedMs);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);

    if (!g_logFile)
        return;

    va_start(ap, fmt);
    fprintf(g_logFile, "[%7lld ms] ", elapsedMs);
    vfprintf(g_logFile, fmt, ap);
    fputc('\n', g_logFile);
    fflush(g_logFile);
    va_end(ap);
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg && !qEnvironmentVariableIsSet("JELLYFIN_NATIVE_VERBOSE_QT")) {
        const QString category = context.category ? QString::fromLatin1(context.category) : QString();
        if (message.startsWith(QStringLiteral("Detected locale \"C\""))
            || (category == QStringLiteral("qt.qpa.wayland")
                && message.contains(QStringLiteral("\"wl-shell\" is a deprecated shell extension"))))
            return;
    }

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

    const QByteArray local = JellyfinNative::sanitizedLogMessage(message).toLocal8Bit();
    if (context.category && context.category[0])
        logLine("[qt:%s] %s: %s", level, context.category, local.constData());
    else
        logLine("[qt:%s] %s", level, local.constData());

    if (type == QtFatalMsg)
        abort();
}

void logQmlWarnings(const QList<QQmlError>& warnings)
{
    for (const QQmlError& warning : warnings)
        logLine("[qml] %s", qPrintable(warning.toString()));
}

#ifndef JELLYFIN_NATIVE_WEBOS
QIcon applicationIcon(bool playerSelected)
{
    const QString variant = playerSelected ? QStringLiteral("spool-film") : QStringLiteral("spool");
    return QIcon(QStringLiteral(":/icons/%1.svg").arg(variant));
}
#endif

} // namespace

int main(int argc, char **argv)
{
#ifdef Q_OS_UNIX
    umask(S_IRWXG | S_IRWXO);
#endif
    const JellyfinNative::ProcessStartupTiming processStartupTiming = JellyfinNative::captureProcessStartupTiming();
    g_startupTimer.start();
    QElapsedTimer& startupTimer = g_startupTimer;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Spool for Jellyfin %s\n", kAppVersion);
            return 0;
        }
    }

    const QString appRootPath = JellyfinNative::resolveAppRoot(argv[0]);
    if (appRootPath.isEmpty())
        return 1;

    g_logFile = openAppLogFile(appRootPath);
    setvbuf(stderr, nullptr, _IOLBF, 0);
    if (g_logFile)
        setvbuf(g_logFile, nullptr, _IOLBF, 0);

    logLine("%s starting", kAppId);
    logLine("startup: exec_to_main_ms=%lld static_init_ms=%.2f",
        static_cast<long long>(processStartupTiming.execToMainMs), processStartupTiming.staticInitializationMs);
    if (!g_logPath.isEmpty())
        logLine("log file: %s", g_logPath.constData());

    // libmpv parses option strings (and many internal numeric values) with the
    // C locale assumption — under any other LC_NUMERIC playback fails to start
    // because option parsing rejects floating-point arguments. Force LC_NUMERIC
    // to C for both this process and any inherited child env so the user does
    // not have to set LC_NUMERIC=C themselves.
    setlocale(LC_NUMERIC, "C");
    qputenv("LC_NUMERIC", QByteArrayLiteral("C"));
    if (setlocale(LC_CTYPE, "C.UTF-8")) {
        qputenv("LANG", QByteArrayLiteral("C.UTF-8"));
        qputenv("LC_CTYPE", QByteArrayLiteral("C.UTF-8"));
    } else if (setlocale(LC_CTYPE, "en_US.UTF-8")) {
        qputenv("LANG", QByteArrayLiteral("en_US.UTF-8"));
        qputenv("LC_CTYPE", QByteArrayLiteral("en_US.UTF-8"));
    }

    if (!JellyfinNative::configurePlatformEnvironment(appRootPath))
        return 1;
    if (!qgetenv("QSG_RENDER_LOOP").isEmpty())
        logLine("QSG_RENDER_LOOP=%s", qgetenv("QSG_RENDER_LOOP").constData());

    const QString cachePath = JellyfinNative::startupCacheRoot(appRootPath);
    configurePersistentStartupCaches(cachePath);

    logLine("app root: %s", qPrintable(appRootPath));
    logLine("QT_QPA_PLATFORM=%s", qgetenv("QT_QPA_PLATFORM").constData());
    logLine("QT_PLUGIN_PATH=%s", qgetenv("QT_PLUGIN_PATH").constData());
    logLine("QML2_IMPORT_PATH=%s", qgetenv("QML2_IMPORT_PATH").constData());
    if (!qgetenv("QT_IM_MODULE").isEmpty())
        logLine("QT_IM_MODULE=%s", qgetenv("QT_IM_MODULE").constData());

    qInstallMessageHandler(qtMessageHandler);
    QLoggingCategory::setFilterRules(QStringLiteral("qt.*.debug=false\nqt.*.info=false"));

    // MpvVideoItem renders into an FBO via libmpv's OpenGL render API; force
    // Qt Quick to use the OpenGL RHI backend (Qt 6 defaults to Vulkan/Metal
    // on some platforms).
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QSurfaceFormat::setDefaultFormat(JellyfinNative::platformSurfaceFormat());

    logLine("startup: constructing QGuiApplication");
    QGuiApplication app(argc, argv);
    JellyfinNative::TerminationSignalHandler terminationSignals(app);
    logLine("startup: QGuiApplication constructed");
    app.setApplicationName(QStringLiteral("Spool for Jellyfin"));
    app.setApplicationVersion(QString::fromLatin1(kAppVersion));
    app.setOrganizationName(QStringLiteral("sachk"));
    app.setApplicationDisplayName(QStringLiteral("Spool for Jellyfin"));
#ifndef JELLYFIN_NATIVE_WEBOS
    const QIcon defaultApplicationIcon = applicationIcon(false);
    const QIcon playerApplicationIcon = applicationIcon(true);
    app.setWindowIcon(defaultApplicationIcon);
    app.setDesktopFileName(QStringLiteral("com.sachk.spool"));
#endif
    const auto& capabilities = JellyfinNative::platformCapabilities();

    const QString diagnosticsRoot = qEnvironmentVariableIsSet("JELLYFIN_DIAGNOSTICS_DIR")
        ? QString::fromLocal8Bit(qgetenv("JELLYFIN_DIAGNOSTICS_DIR"))
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/diagnostics");
    JellyfinNative::Diagnostics::initialize(QString::fromLatin1(kAppId), diagnosticsRoot);
    JellyfinNative::Diagnostics::EventLoopWatchdog eventLoopWatchdog(&app);

    const QStringList arguments = app.arguments();
    const bool smokeAndExit = arguments.contains(QStringLiteral("--smoke-and-exit"));
    if (arguments.contains(QStringLiteral("--diagnose-and-exit"))
        || arguments.contains(QStringLiteral("--dump-diagnostics"))) {
        JellyfinNative::Diagnostics::dumpDiagnostics(QStringLiteral("command-line"));
        JellyfinNative::Diagnostics::shutdown();
        return 0;
    }
    if (qEnvironmentVariableIntValue("JELLYFIN_DIAGNOSTICS_BLOCK_GUI_MS") > 0) {
        const int blockMs = qEnvironmentVariableIntValue("JELLYFIN_DIAGNOSTICS_BLOCK_GUI_MS");
        QTimer::singleShot(1000, &app, [blockMs]() {
            JellyfinNative::Diagnostics::logEvent(QStringLiteral("simulation"), QStringLiteral("block_gui_begin"),
                { { QStringLiteral("durationMs"), blockMs } });
            QThread::msleep(static_cast<unsigned long>(blockMs));
            JellyfinNative::Diagnostics::logEvent(QStringLiteral("simulation"), QStringLiteral("block_gui_end"));
        });
    }

    const JellyfinNative::MemoryBudget memoryBudget = JellyfinNative::MemoryBudget::detect();
    logLine("memory budget: memTotal=%lld networkDisk=%lld qmlImageDisk=%lld artworkBytes=%d demuxer=%s/%s",
        static_cast<long long>(memoryBudget.memTotalBytes), static_cast<long long>(memoryBudget.networkDiskCacheBytes),
        static_cast<long long>(memoryBudget.qmlImageDiskCacheBytes), memoryBudget.artworkByteCacheBytes,
        memoryBudget.mpvDemuxerMaxBytes.constData(), memoryBudget.mpvDemuxerMaxBackBytes.constData());

    auto *networkAccessManager = new QNetworkAccessManager(&app);
    auto *diskCache = new QNetworkDiskCache(networkAccessManager);
    const QString qmlImageCachePath = cachePath + QStringLiteral("/qml-image-cache");
    QDir().mkpath(cachePath);
    diskCache->setCacheDirectory(cachePath + QStringLiteral("/network-cache"));
    diskCache->setMaximumCacheSize(memoryBudget.networkDiskCacheBytes);
    networkAccessManager->setCache(diskCache);

    JellyfinNative::DatabaseManager database;
    QObject::connect(
        &database, &JellyfinNative::DatabaseManager::initializationFailed, &app, [&app](const QString& message) {
            logLine("database initialization failed: %s", qPrintable(message));
            app.exit(1);
        });
    JellyfinNative::InputLatencyMonitor inputLatencyMonitor;
    JellyfinNative::SystemPerformanceMonitor systemPerformanceMonitor;
    systemPerformanceMonitor.setAudioDecodeCpuTimeProvider(
        [] { return JellyfinNative::platformAudioDecodeCpuTimeNs(); });
    JellyfinNative::NativeAppWindow window(QString::fromLatin1(kAppId));
    window.setSystemMemoryBytes(memoryBudget.memTotalBytes);
    JellyfinNative::configurePlatformWindow(window);
    inputLatencyMonitor.attachWindow(&window);
    window.setInputLatencyMonitor(&inputLatencyMonitor);
    const auto directSingleShot = static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::SingleShotConnection);
    const auto traceFirstFrameSignal = [&window, &startupTimer, directSingleShot](auto signal, const char *name) {
        QObject::connect(
            &window, signal, &window,
            [&window, &startupTimer, name] {
                const qint64 elapsedMs = startupTimer.elapsed();
                QMetaObject::invokeMethod(
                    &window, [elapsedMs, name] { logLine("startup: first frame %s at %lld ms", name, elapsedMs); },
                    Qt::QueuedConnection);
            },
            directSingleShot);
    };
    traceFirstFrameSignal(&QQuickWindow::beforeFrameBegin, "begin");
    traceFirstFrameSignal(&QQuickWindow::beforeSynchronizing, "sync_begin");
    traceFirstFrameSignal(&QQuickWindow::afterSynchronizing, "sync_end");
    traceFirstFrameSignal(&QQuickWindow::beforeRendering, "render_begin");
    traceFirstFrameSignal(&QQuickWindow::afterRendering, "render_end");
    traceFirstFrameSignal(&QQuickWindow::frameSwapped, "swapped");
    traceFirstFrameSignal(&QQuickWindow::afterFrameEnd, "end");
    configurePersistentRhiPipelineCache(window, cachePath);
    {
        JellyfinNative::Diagnostics::Phase phase(QStringLiteral("startup"), QStringLiteral("prepare_ui_surface"));
        if (!window.prepareForUiSurface()) {
            logLine("failed to initialize the native UI surface");
            return 1;
        }
    }
    logLine("startup: prepareForUiSurface completed in %lld ms", static_cast<long long>(startupTimer.elapsed()));

    // Start the SQLite worker before constructing the controllers. Device
    // identity and session reads are awaited after the first frame.
    {
        JellyfinNative::Diagnostics::Phase phase(QStringLiteral("startup"), QStringLiteral("database_initialize"));
        const QString databasePath = JellyfinNative::persistentDataRoot() + QStringLiteral("/cache.sqlite");
        if (!database.initialize(databasePath))
            return 1;
    }

    JellyfinNative::TlsTrustController tlsTrust;
    auto discovery = std::make_unique<JellyfinNative::DiscoveryController>(&tlsTrust);
    auto api = std::make_unique<JellyfinNative::JellyfinApiFacade>(networkAccessManager, &tlsTrust);
    api->setDeviceIdentity({}, capabilities.deviceName, QString::fromLatin1(kAppVersion));
    JellyfinNative::configurePlatformPlaybackCapabilities(*api, app);

    const JellyfinNative::CpuTopology cpuTopology = JellyfinNative::detectCpuTopology();
    logLine("artwork: cpu logical=%d physical=%d smt=%s source=%s decodeThreads=%d", cpuTopology.logicalCpus,
        cpuTopology.physicalCores, cpuTopology.smtDetected ? "true" : "false", qPrintable(cpuTopology.source),
        cpuTopology.artworkDecodeThreads);
    auto artworkService = std::make_unique<JellyfinNative::ArtworkService>(
        qmlImageCachePath + QStringLiteral("/artwork"), memoryBudget.qmlImageDiskCacheBytes,
        memoryBudget.artworkByteCacheBytes, cpuTopology.artworkDecodeThreads, &tlsTrust);
    artworkService->setUiWidth(window.width());

    auto player = std::make_unique<JellyfinNative::PlayerController>(&window, api.get(), &tlsTrust);
    player->setDemuxerBudget(memoryBudget.mpvDemuxerMaxBytes, memoryBudget.mpvDemuxerMaxBackBytes);
    JellyfinNative::ScreenSaverInhibitor screenSaverInhibitor;
    const auto updateScreenSaver = [&screenSaverInhibitor, player = player.get()] {
        screenSaverInhibitor.setInhibited(JellyfinNative::screenSaverShouldBeInhibited(
            player && player->sessionActive(), player && player->paused()));
    };
    QObject::connect(player.get(), &JellyfinNative::PlayerController::playbackStateChanged, &app, updateScreenSaver);
    QObject::connect(player.get(), &JellyfinNative::PlayerController::sessionActiveChanged, &app, updateScreenSaver);
#ifndef JELLYFIN_NATIVE_WEBOS
    const auto updateApplicationIcon
        = [&app, &window, player = player.get(), &defaultApplicationIcon, &playerApplicationIcon] {
              const QIcon& icon = player->sessionActive() ? playerApplicationIcon : defaultApplicationIcon;
              app.setWindowIcon(icon);
              window.setIcon(icon);
          };
    QObject::connect(
        player.get(), &JellyfinNative::PlayerController::sessionActiveChanged, &app, updateApplicationIcon);
    updateApplicationIcon();
#endif
    auto controller = std::make_unique<JellyfinNative::AppController>(
        &database, discovery.get(), api.get(), artworkService.get(), player.get(), &tlsTrust);
    QObject::connect(controller.get(), &JellyfinNative::AppController::clearLogsRequested, &app, [appRootPath]() {
        const std::lock_guard lock(g_logMutex);
        if (g_logFile) {
            fclose(g_logFile);
            g_logFile = nullptr;
        }
        const QByteArray fileName = QFile::encodeName(JellyfinNative::appLogFileName());
        for (const QString& directory : JellyfinNative::appLogDirectories(appRootPath)) {
            if (directory.isEmpty())
                continue;
            const QString path = QDir(directory).filePath(QString::fromUtf8(fileName));
            QFile::remove(path);
            QFile::remove(path + QStringLiteral(".1"));
            QFile::remove(path + QStringLiteral(".2"));
        }
        g_logFile = openAppLogFile(appRootPath);
    });
    // A desktop close event arrives while the scene graph is still rendering.
    // Tear down here so the mpv render-context handoff completes immediately;
    // aboutToQuit is too late because the window no longer produces frames.
    QObject::connect(
        &window, &JellyfinNative::NativeAppWindow::closeRequested, controller.get(),
        [controller = controller.get()]() {
            logLine("window close requested: stopping controllers");
            controller->shutdown();
        },
        Qt::DirectConnection);

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
            JellyfinNative::Diagnostics::logEvent(QStringLiteral("simulation"), QStringLiteral("shutdown_hang_begin"),
                { { QStringLiteral("durationMs"), hangMs } });
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

    // Parented to the engine so it outlives every incubator and dies with it.
    window.engine()->setIncubationController(new BoostedIncubationController(window.engine()));
    window.engine()->addImageProvider(
        QStringLiteral("artwork"), new JellyfinNative::ArtworkImageProvider(artworkService.get()));
    window.engine()->addImageProvider(QStringLiteral("mpv-overlay"), window.createOverlayImageProvider());
    window.engine()->addImportPath(appRootPath + QStringLiteral("/qt-qml"));
    QObject::connect(window.engine(), &QQmlEngine::warnings, &logQmlWarnings);
    QObject::connect(&window, &QQuickView::statusChanged,
        [](QQuickView::Status status) { logLine("view status changed: %d", static_cast<int>(status)); });
    auto localization = std::make_unique<JellyfinNative::LocalizationManager>();
    localization->attachToEngine(window.engine());
    if (api) {
        api->setAcceptLanguage(localization->bcp47Locale());
        QObject::connect(localization.get(), &JellyfinNative::LocalizationManager::localeChanged, api.get(),
            [api = api.get(), loc = localization.get()]() { api->setAcceptLanguage(loc->bcp47Locale()); });
    }
    auto router = std::make_unique<JellyfinNative::RouterController>();
    JellyfinNative::PlatformApplicationServices platformServices(app, window, *controller, *router);
    platformServices.start();
    QQmlPropertyMap *platformInfo = QQmlPropertyMap::create(&app);
    platformInfo->insert(QStringLiteral("isTV"), capabilities.isTV);
    platformInfo->insert(QStringLiteral("isWebOS"), capabilities.isWebOS);
    platformInfo->insert(QStringLiteral("hasSystemFonts"), capabilities.hasSystemFonts);
    platformInfo->insert(QStringLiteral("hasDesktopPointer"), capabilities.hasDesktopPointer);
    platformInfo->insert(QStringLiteral("supportsMpvConfig"), capabilities.supportsMpvConfig);
    platformInfo->insert(QStringLiteral("usesPerOutputAudioDelay"), capabilities.usesPerOutputAudioDelay);
    platformInfo->insert(QStringLiteral("deviceName"), capabilities.deviceName);
    platformInfo->insert(QStringLiteral("rendererName"), capabilities.rendererName);
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "App", controller.get());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Art", artworkService.get());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Browse", controller->browse());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Home", controller->home());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Content", controller->content());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "ItemState", controller->itemState());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Search", controller->search());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Libraries", controller->libraries());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "DiscoveredServers", controller->discoveredServers());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Discovery", discovery.get());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "TlsTrust", &tlsTrust);
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Session", controller->session());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "QuickConnect", controller->quickConnect());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Settings", controller->settings());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Player", controller->player());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "PlayQueue", controller->playQueue());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "SyncPlay", controller->syncPlay());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Management", controller->management());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Router", router.get());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "NativeWindow", &window);
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "InputLatency", &inputLatencyMonitor);
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "SystemPerformance", &systemPerformanceMonitor);
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "I18n", localization.get());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Platform", platformInfo);
    qmlRegisterType<JellyfinNative::MpvVideoItem>("JellyfinWebOS", 1, 0, "MpvVideoItem");
    // Start asynchronous device, settings, account, and discovery reads before
    // QML construction. A sole saved account is resolved before routing begins.
    controller->initialize();

    QObject::connect(
        &app, &QCoreApplication::aboutToQuit, router.get(), [router = router.get()] { router->markCleanShutdown(); });

    {
        JellyfinNative::Diagnostics::Phase phase(QStringLiteral("startup"), QStringLiteral("load_qml"));
        // Load through the module registry so AOT QML units are used and
        // constrained devices do not retain compiler buffers.
        window.loadFromModule("JellyfinWebOS", "Main");
    }
    if (window.status() == QQuickView::Error) {
        logQmlWarnings(window.errors());
        return 1;
    }
    logLine("startup: QML source loaded in %lld ms", static_cast<long long>(startupTimer.elapsed()));

    if (smokeAndExit) {
        logLine("startup smoke completed without showing window");
        JellyfinNative::Diagnostics::setInstanceState(QStringLiteral("startup_smoke_complete"));
        window.setSource(QUrl());
        JellyfinNative::Diagnostics::shutdown();
        return 0;
    }

    QTimer::singleShot(1000, router.get(), [router = router.get()] { router->beginSession(false); });

    QTimer::singleShot(0, &window, [&startupTimer]() {
        logLine("startup: first event-loop turn at %lld ms", static_cast<long long>(startupTimer.elapsed()));
        JellyfinNative::Diagnostics::setInstanceState(QStringLiteral("running"));
    });

    logLine("startup: entering event loop at %lld ms", static_cast<long long>(startupTimer.elapsed()));
    const int exitCode = app.exec();
    logLine("app.exec returned: %d", exitCode);
    JellyfinNative::Diagnostics::setInstanceState(
        QStringLiteral("app_exec_returned"), { { QStringLiteral("exitCode"), exitCode } });
    JellyfinNative::Diagnostics::shutdown();
    return exitCode;
}

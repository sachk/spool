#include "api/JellyfinApiFacade.h"
#include "app/AppController.h"
#include "app/ArtworkImageProvider.h"
#include "app/CpuTopology.h"
#include "app/LocalizationManager.h"
#include "app/MemoryBudget.h"
#include "app/NativeAppWindow.h"
#include "app/RouterController.h"
#include "app/UserItemStateController.h"
#include "cache/DatabaseManager.h"
#include "common/LogRotation.h"
#include "diagnostics/Diagnostics.h"
#include "diagnostics/InputLatencyMonitor.h"
#include "discovery/DiscoveryController.h"
#include "player/MpvVideoItem.h"
#include "player/PlayerController.h"
#ifdef JELLYFIN_NATIVE_WEBOS
#include "player/MpvRuntime.h"
#endif

extern "C" {
#ifndef Q_OS_WIN
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef JELLYFIN_NATIVE_WEBOS
#include <alsa/asoundlib.h>
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
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QLoggingCategory>
#include <QMessageLogContext>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQmlPropertyMap>
#include <QQuickGraphicsConfiguration>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSocketNotifier>
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

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr auto kAppId = "com.sachk.tern";
constexpr auto kAppVersion = JELLYFIN_VERSION;
#ifdef JELLYFIN_NATIVE_WEBOS
constexpr auto kDefaultLogDir = "/tmp";
constexpr auto kAppLogFileName = "com.sachk.tern.log";
#endif

FILE *g_logFile = nullptr;
QByteArray g_logPath;
QElapsedTimer g_startupTimer;
qint64 g_staticInitializationNs = 0;

#ifndef Q_OS_WIN
__attribute__((constructor)) void recordStaticInitializationStart()
{
    timespec value {};
    if (clock_gettime(CLOCK_MONOTONIC, &value) == 0)
        g_staticInitializationNs = static_cast<qint64>(value.tv_sec) * 1000000000LL + value.tv_nsec;
}
#endif

#ifdef JELLYFIN_NATIVE_WEBOS
// Window pointer captured for the LS2 lifecycle callback, which runs on
// the LS2 dispatch thread. We marshal events onto the GUI thread via
// QMetaObject::invokeMethod against this object.
JellyfinNative::NativeAppWindow *g_lifecycleWindow = nullptr;
JellyfinNative::AppController *g_appController = nullptr;
std::atomic_uint64_t g_soundOutputEventGeneration { 0 };
std::mutex g_pendingSoundOutputMutex;
QString g_pendingSoundOutput;
int g_pendingDisplayLatencyMs = -1;
int g_pendingOutputLatencyMs = -1;
#endif

FILE *openRotatedLogFile(const QByteArray& path)
{
    JellyfinNative::rotateLogFile(path.constData());
    return fopen(path.constData(), "w");
}

#ifdef JELLYFIN_NATIVE_WEBOS
QByteArray logPathInDir(const QByteArray& dir, const QByteArray& fileName)
{
    QByteArray path = dir;
    if (!path.endsWith('/'))
        path += '/';
    path += fileName;
    return path;
}

FILE *openLogFileInDir(const QByteArray& dir, const QByteArray& fileName)
{
    QDir().mkpath(QString::fromUtf8(dir));
    const QByteArray path = logPathInDir(dir, fileName);
    if (FILE *file = openRotatedLogFile(path)) {
        g_logPath = path;
        qputenv("JELLYFIN_NATIVE_LOG_DIR", dir);
        return file;
    }
    return nullptr;
}

#endif

FILE *openAppLogFile(const QString& appRootPath)
{
#ifdef JELLYFIN_NATIVE_WEBOS
    if (FILE *file = openLogFileInDir(QByteArray(kDefaultLogDir), QByteArray(kAppLogFileName)))
        return file;

    const QString fallbackDir = QDir(appRootPath).filePath(QStringLiteral(".cache/logs"));
    const QByteArray encodedFallbackDir = QFile::encodeName(fallbackDir);
    return openLogFileInDir(encodedFallbackDir, QByteArray(kAppLogFileName));
#else
    Q_UNUSED(appRootPath);
#ifdef Q_OS_WIN
    const QString dataRoot
        = QString::fromLocal8Bit(qgetenv("LOCALAPPDATA")) + QLatin1Char('/') + QString::fromLatin1(kAppId);
#else
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#endif
    const QString logDir = dataRoot + QStringLiteral("/logs");
    QDir().mkpath(logDir);
    const QByteArray preferred = QFile::encodeName(QDir(logDir).filePath(QStringLiteral("jellyfin-native.log")));
    if (FILE *file = openRotatedLogFile(preferred)) {
        g_logPath = preferred;
        qputenv("JELLYFIN_NATIVE_LOG_DIR", QFile::encodeName(logDir));
        return file;
    }
    return nullptr;
#endif
}

QString resolveAppRoot(const char *argv0)
{
#ifdef __APPLE__
    char buffer[PATH_MAX];
    const size_t size = sizeof(buffer);
    uint32_t length = static_cast<uint32_t>(size);
    if (_NSGetExecutablePath(buffer, &length) != 0)
        return {};
    char resolved[PATH_MAX];
    if (realpath(buffer, resolved)) {
        if (strlen(resolved) >= size)
            return {};
        strcpy(buffer, resolved);
    }
#elif !defined(Q_OS_WIN)
    char buffer[PATH_MAX];
    const size_t size = sizeof(buffer);
    const ssize_t length = readlink("/proc/self/exe", buffer, size - 1);
    if (length < 0)
        return {};
    buffer[length] = '\0';
#else
    Q_UNUSED(argv0);
    wchar_t executablePath[32768] {};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath, static_cast<DWORD>(std::size(executablePath)));
    if (length == 0 || length >= std::size(executablePath))
        return {};
    const QString path = QString::fromWCharArray(executablePath, static_cast<qsizetype>(length));
    const qsizetype separator = path.lastIndexOf(QLatin1Char('\\'));
    return separator > 0 ? path.left(separator) : QString();
#endif

#ifndef Q_OS_WIN
    char *lastSlash = strrchr(buffer, '/');
    if (!lastSlash)
        return {};
    *lastSlash = '\0';
    lastSlash = strrchr(buffer, '/');
    if (!lastSlash)
        return {};
    *lastSlash = '\0';
    return QString::fromUtf8(buffer);
#endif
}

QString startupCacheRoot(const QString& appRootPath)
{
    const QByteArray configured = qgetenv("JELLYFIN_NATIVE_CACHE_HOME");
    if (!configured.isEmpty())
        return QString::fromLocal8Bit(configured);

    const QByteArray xdgCache = qgetenv("XDG_CACHE_HOME");
    if (!xdgCache.isEmpty())
        return QDir(QString::fromLocal8Bit(xdgCache)).filePath(QString::fromLatin1(kAppId));

#ifdef JELLYFIN_NATIVE_WEBOS
    return QDir(appRootPath).filePath(QStringLiteral(".cache"));
#else
    Q_UNUSED(appRootPath);
#ifdef Q_OS_WIN
    const QString cacheRoot = QString::fromLocal8Bit(qgetenv("LOCALAPPDATA")) + QLatin1Char('/')
        + QString::fromLatin1(kAppId) + QStringLiteral("/cache");
#else
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#endif
    return cacheRoot;
#endif
}

QString persistentDataRoot()
{
#ifdef Q_OS_WIN
    return QString::fromLocal8Bit(qgetenv("LOCALAPPDATA")) + QLatin1Char('/') + QString::fromLatin1(kAppId);
#else
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
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
            if (snprintf(candidate, sizeof(candidate), "%s/wayland-0", runtimeDir) < static_cast<int>(sizeof(candidate))
                && access(candidate, F_OK) == 0) {
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
#ifndef Q_OS_WIN
int g_signalPipe[2] = { -1, -1 };

void handleSignal(int signalNumber)
{
    const char byte = static_cast<char>(signalNumber > 0 ? signalNumber : 1);
    // write() is async-signal-safe; ignore the result — if the pipe is full
    // the previous byte already armed the notifier.
    ssize_t n = write(g_signalPipe[1], &byte, 1);
    (void)n;
}
#endif

void logLine(const char *fmt, ...)
{
    const long long elapsedMs = g_startupTimer.isValid() ? static_cast<long long>(g_startupTimer.elapsed()) : 0;

    va_list ap;
#ifndef Q_OS_WIN
    va_start(ap, fmt);
    fprintf(stderr, "[%7lld ms] ", elapsedMs);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
#endif

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

    const QByteArray local = message.toLocal8Bit();
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
            QMetaObject::invokeMethod(
                window,
                [window]() {
                    logLine("[ls2-lifecycle] relaunch -> bringToFront");
                    window->bringToFront();
                },
                Qt::QueuedConnection);
        }
    } else if (event == QStringLiteral("close")) {
        const QString reason = doc.object().value(QStringLiteral("reason")).toString();
        if (reason == QStringLiteral("memoryReclaim")) {
            logLine("[ls2-lifecycle] memoryReclaim close ignored");
            return true;
        }
        QMetaObject::invokeMethod(
            qApp,
            []() {
                logLine("[ls2-lifecycle] close -> QCoreApplication::quit");
                QCoreApplication::quit();
            },
            Qt::QueuedConnection);
    }

    return true;
}

bool lunaMemoryStatusCallback(LSHandle *, LSMessage *message, void *)
{
    const char *payload = message ? LSMessageGetPayload(message) : nullptr;
    if (!payload)
        return true;

    const QByteArray raw = QByteArray::fromRawData(payload, static_cast<int>(strlen(payload)));
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        logLine("[ls2-memory] %s", payload);
        return true;
    }

    const QJsonObject object = doc.object();
    if (!object.value(QStringLiteral("returnValue")).toBool(true)
        && object.value(QStringLiteral("errorText")).toString().contains(QStringLiteral("Service does not exist"))) {
        static bool loggedUnavailable = false;
        if (!loggedUnavailable) {
            logLine("[ls2-memory] service unavailable (non-fatal)");
            loggedUnavailable = true;
        }
        return true;
    }

    logLine("[ls2-memory] %s", payload);
    const QString level = object.value(QStringLiteral("level")).toString();
    if (level.isEmpty() || !g_appController)
        return true;
    JellyfinNative::AppController *controller = g_appController;
    QMetaObject::invokeMethod(
        controller, [controller, level]() { controller->onMemoryPressure(level); }, Qt::QueuedConnection);
    return true;
}

int readAlsaControlLastInteger(snd_ctl_t *control, unsigned int numid)
{
    snd_ctl_elem_id_t *id = nullptr;
    snd_ctl_elem_info_t *info = nullptr;
    snd_ctl_elem_value_t *value = nullptr;
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_value_alloca(&value);
    snd_ctl_elem_id_set_numid(id, numid);
    snd_ctl_elem_info_set_id(info, id);
    if (snd_ctl_elem_info(control, info) < 0)
        return -1;
    snd_ctl_elem_value_set_id(value, id);
    if (snd_ctl_elem_read(control, value) < 0)
        return -1;

    const unsigned int count = snd_ctl_elem_info_get_count(info);
    if (count == 0)
        return -1;
    return static_cast<int>(snd_ctl_elem_value_get_integer(value, count - 1));
}

struct WebOSAudioLatencySnapshot {
    int displayLatencyMs = -1;
    int outputLatencyMs = -1;
};

WebOSAudioLatencySnapshot readWebOSAudioLatency()
{
    WebOSAudioLatencySnapshot snapshot;
    snd_ctl_t *control = nullptr;
    const int result = snd_ctl_open(&control, "hw:0", 0);
    if (result < 0) {
        logLine("[ls2-audio] snd_ctl_open failed: %s", snd_strerror(result));
        return snapshot;
    }

    // These are the controls identified on the target TV during the latency
    // experiment. The final integer is the active route's latency in ms.
    snapshot.displayLatencyMs = readAlsaControlLastInteger(control, 62); // Adec Lipsync Offset
    snapshot.outputLatencyMs = readAlsaControlLastInteger(control, 96); // Audio Latency Time
    snd_ctl_close(control);
    return snapshot;
}

bool lunaSoundOutputCallback(LSHandle *, LSMessage *message, void *)
{
    const char *payload = message ? LSMessageGetPayload(message) : nullptr;
    if (!payload)
        return true;

    const QByteArray raw = QByteArray::fromRawData(payload, static_cast<int>(strlen(payload)));
    const QJsonDocument document = QJsonDocument::fromJson(raw);
    const QString output
        = document.isObject() ? document.object().value(QStringLiteral("soundOutput")).toString() : QString();
    if (output.isEmpty()) {
        logLine("[ls2-audio] %s", payload);
        return true;
    }

    const WebOSAudioLatencySnapshot latency = readWebOSAudioLatency();
    logLine("[ls2-audio] soundOutput=%s adecLipsyncMs=%d outputLatencyMs=%d payload=%s", qPrintable(output),
        latency.displayLatencyMs, latency.outputLatencyMs, payload);

    {
        const std::lock_guard lock(g_pendingSoundOutputMutex);
        g_pendingSoundOutput = output;
        g_pendingDisplayLatencyMs = latency.displayLatencyMs;
        g_pendingOutputLatencyMs = latency.outputLatencyMs;
    }
    ++g_soundOutputEventGeneration;
    return true;
}
#endif

} // namespace

int main(int argc, char **argv)
{
#ifndef Q_OS_WIN
    timespec mainTimestamp {};
    clock_gettime(CLOCK_MONOTONIC, &mainTimestamp);
    const qint64 mainNs = static_cast<qint64>(mainTimestamp.tv_sec) * 1000000000LL + mainTimestamp.tv_nsec;
#endif
    g_startupTimer.start();
    QElapsedTimer& startupTimer = g_startupTimer;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Jellyfin Native %s\n", kAppVersion);
            return 0;
        }
    }

    const QString appRootPath = resolveAppRoot(argv[0]);
    if (appRootPath.isEmpty())
        return 1;

    g_logFile = openAppLogFile(appRootPath);
#ifndef Q_OS_WIN
    setvbuf(stderr, nullptr, _IOLBF, 0);
#endif
    if (g_logFile) {
#ifdef Q_OS_WIN
        setvbuf(g_logFile, nullptr, _IONBF, 0);
#else
        setvbuf(g_logFile, nullptr, _IOLBF, 0);
#endif
    }

    logLine("%s starting", kAppId);
#ifndef Q_OS_WIN
    QFile statFile(QStringLiteral("/proc/self/stat"));
    QFile uptimeFile(QStringLiteral("/proc/uptime"));
    qint64 execToMainMs = -1;
    if (statFile.open(QIODevice::ReadOnly) && uptimeFile.open(QIODevice::ReadOnly)) {
        const QByteArray stat = statFile.readAll();
        const qsizetype commEnd = stat.lastIndexOf(')');
        const QList<QByteArray> fields = commEnd >= 0 ? stat.mid(commEnd + 2).split(' ') : QList<QByteArray> {};
        const QByteArray uptimeText = uptimeFile.readAll().split(' ').value(0);
        bool startOk = false;
        bool uptimeOk = false;
        const qulonglong startTicks = fields.value(19).toULongLong(&startOk);
        const double uptimeSeconds = uptimeText.toDouble(&uptimeOk);
        const long ticksPerSecond = sysconf(_SC_CLK_TCK);
        if (startOk && uptimeOk && ticksPerSecond > 0)
            execToMainMs = qRound64((uptimeSeconds - static_cast<double>(startTicks) / ticksPerSecond) * 1000.0);
    }
    logLine("startup: exec_to_main_ms=%lld static_init_ms=%.2f", static_cast<long long>(execToMainMs),
        g_staticInitializationNs > 0 ? static_cast<double>(mainNs - g_staticInitializationNs) / 1000000.0 : -1.0);
#endif
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

#ifdef JELLYFIN_NATIVE_WEBOS
    setenv("APPID", kAppId, 1);
    setenv("MALLOC_ARENA_MAX", "2", 0);
    setenv("DISPLAY_ID", "0", 1);
    setenv("STARFISH_AUDIO_HINT", "0", 1);
    setenv("QT_QPA_PLATFORM", "wayland-egl", 1);
    setenv("QSG_RHI_BACKEND", "opengl", 1);
    // Some webOS shells export legacy Qt 5 scenegraph/input-module knobs.
    // Qt 6 probes those names as plugins and logs noisy startup warnings before
    // falling back. The app selects OpenGL through Qt 6 RHI knobs, so drop the
    // inherited client-side overrides.
    unsetenv("QT_QUICK_BACKEND");
    unsetenv("QMLSCENE_DEVICE");
    unsetenv("QT_IM_MODULES");
    // Drive the stock webOS on-screen keyboard (MaliitServer) through the
    // compositor's text_model protocol via our statically linked input
    // context plugin (src/platform/webos/WebOSInputContext.cpp).
    setenv("QT_IM_MODULE", "webosim", 1);
    setenv("QT_WAYLAND_SHELL_INTEGRATION", "wl-shell", 1);
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
        setenv("QT_LOGGING_RULES", "qt.qml*=true;qt.qpa*=true;qt.scenegraph*=true;qt.quick*=true;qt.plugin*=true", 1);
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
    qputenv("QT_QPA_PLATFORM",
        qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArrayLiteral("wayland"));
#endif
    if (qEnvironmentVariableIsSet("JELLYFIN_NATIVE_VERBOSE_QT")) {
        qputenv("QT_DEBUG_PLUGINS", QByteArrayLiteral("1"));
        qputenv("QT_LOGGING_RULES",
            QByteArrayLiteral("qt.qml*=true;qt.qpa*=true;qt.scenegraph*=true;qt.quick*=true;qt.plugin*=true"));
    }
#endif

    const QString cachePath = startupCacheRoot(appRootPath);
    configurePersistentStartupCaches(cachePath);

    logLine("app root: %s", qPrintable(appRootPath));
    logLine("QT_QPA_PLATFORM=%s", qgetenv("QT_QPA_PLATFORM").constData());
    logLine("QT_PLUGIN_PATH=%s", qgetenv("QT_PLUGIN_PATH").constData());
    logLine("QML2_IMPORT_PATH=%s", qgetenv("QML2_IMPORT_PATH").constData());
#ifdef JELLYFIN_NATIVE_WEBOS
    logLine("QT_IM_MODULE=%s", qgetenv("QT_IM_MODULE").constData());
#endif

    qInstallMessageHandler(qtMessageHandler);
    QLoggingCategory::setFilterRules(QStringLiteral("qt.*.debug=false\nqt.*.info=false"));

#ifndef Q_OS_WIN
    if (pipe(g_signalPipe) == 0) {
        // Non-blocking write so the signal handler never stalls; reads on the
        // notifier side are also non-blocking via QSocketNotifier semantics.
        for (int fd : { g_signalPipe[0], g_signalPipe[1] }) {
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
#endif

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
    app.setOrganizationName(QStringLiteral("sachk"));
    app.setApplicationDisplayName(QStringLiteral("Jellyfin Native"));

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

    std::unique_ptr<QSocketNotifier> signalNotifier;
#ifndef Q_OS_WIN
    if (g_signalPipe[0] >= 0) {
        signalNotifier = std::make_unique<QSocketNotifier>(g_signalPipe[0], QSocketNotifier::Read);
        QObject::connect(
            signalNotifier.get(), &QSocketNotifier::activated, &app, [&app](QSocketDescriptor, QSocketNotifier::Type) {
                char buf[16];
                ssize_t count = 0;
                while ((count = read(g_signalPipe[0], buf, sizeof(buf))) > 0) {
                    for (ssize_t i = 0; i < count; ++i) {
                        JellyfinNative::Diagnostics::logEvent(QStringLiteral("signal"), QStringLiteral("received"),
                            { { QStringLiteral("signal"), static_cast<int>(buf[i]) } });
                    }
                }
                logLine("signal received, quitting");
                app.quit();
            });
    }
#endif

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
    JellyfinNative::NativeAppWindow window(QString::fromLatin1(kAppId));
    inputLatencyMonitor.attachWindow(&window);
    window.setInputLatencyMonitor(&inputLatencyMonitor);
    configurePersistentRhiPipelineCache(window, cachePath);
    {
        JellyfinNative::Diagnostics::Phase phase(QStringLiteral("startup"), QStringLiteral("prepare_ui_surface"));
        if (!window.prepareForUiSurface()) {
            logLine("failed to initialize Qt webOS UI surface");
            return 1;
        }
    }
    logLine("startup: prepareForUiSurface completed in %lld ms", static_cast<long long>(startupTimer.elapsed()));

    // Start the SQLite worker before constructing the controllers. Device
    // identity and session reads are awaited after the first frame.
    {
        JellyfinNative::Diagnostics::Phase phase(QStringLiteral("startup"), QStringLiteral("database_initialize"));
        const QString databasePath = persistentDataRoot() + QStringLiteral("/cache.sqlite");
        if (!database.initialize(databasePath))
            return 1;
    }

    auto discovery = std::make_unique<JellyfinNative::DiscoveryController>();
    auto api = std::make_unique<JellyfinNative::JellyfinApiFacade>(networkAccessManager);
    api->setDeviceIdentity({},
#ifdef JELLYFIN_NATIVE_WEBOS
        QStringLiteral("LG webOS TV"),
#else
        QStringLiteral("Linux Wayland"),
#endif
        QString::fromLatin1(kAppVersion));

    const JellyfinNative::CpuTopology cpuTopology = JellyfinNative::detectCpuTopology();
    logLine("artwork: cpu logical=%d physical=%d smt=%s source=%s decodeThreads=%d", cpuTopology.logicalCpus,
        cpuTopology.physicalCores, cpuTopology.smtDetected ? "true" : "false", qPrintable(cpuTopology.source),
        cpuTopology.artworkDecodeThreads);
    auto artworkService
        = std::make_unique<JellyfinNative::ArtworkService>(qmlImageCachePath + QStringLiteral("/artwork"),
            memoryBudget.qmlImageDiskCacheBytes, memoryBudget.artworkByteCacheBytes, cpuTopology.artworkDecodeThreads);
    artworkService->setUiWidth(window.width());

    auto player = std::make_unique<JellyfinNative::PlayerController>(&window, api.get());
    player->setDemuxerBudget(memoryBudget.mpvDemuxerMaxBytes, memoryBudget.mpvDemuxerMaxBackBytes);
    auto controller = std::make_unique<JellyfinNative::AppController>(
        &database, discovery.get(), api.get(), artworkService.get(), player.get());
#ifdef JELLYFIN_NATIVE_WEBOS
    g_appController = controller.get();
    QObject::connect(
        controller.get(), &JellyfinNative::AppController::aggressiveMemoryPressure, &window,
        [&window]() {
            logLine("memory pressure: releasing QQuickWindow resources");
            window.releaseResources();
        },
        Qt::QueuedConnection);
#endif

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
    QObject::connect(
        &app, &QGuiApplication::applicationStateChanged, &app, [player = player.get()](Qt::ApplicationState state) {
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
    QQmlPropertyMap *platformInfo = QQmlPropertyMap::create(&app);
#ifdef JELLYFIN_NATIVE_WEBOS
    platformInfo->insert(QStringLiteral("isWebOS"), true);
#else
    platformInfo->insert(QStringLiteral("isWebOS"), false);
#endif
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "App", controller.get());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Art", artworkService.get());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Browse", controller->browse());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Home", controller->home());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Content", controller->content());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "ItemState", controller->itemState());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Search", controller->search());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Libraries", controller->libraries());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "DiscoveredServers", controller->discoveredServers());
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
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "I18n", localization.get());
    qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "Platform", platformInfo);
    qmlRegisterType<JellyfinNative::MpvVideoItem>("JellyfinWebOS", 1, 0, "MpvVideoItem");
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
    logLine("startup: QML source loaded in %lld ms", static_cast<long long>(startupTimer.elapsed()));

    if (smokeAndExit) {
        logLine("startup smoke completed without showing window");
        JellyfinNative::Diagnostics::setInstanceState(QStringLiteral("startup_smoke_complete"));
        window.setSource(QUrl());
        JellyfinNative::Diagnostics::shutdown();
        return 0;
    }
#ifdef JELLYFIN_NATIVE_WEBOS
    window.showFullScreen();
#else
    window.show();
#endif
    window.requestActivate();

#ifdef JELLYFIN_NATIVE_WEBOS
    g_lifecycleWindow = &window;

    // Load libmpv (dlopen'd, not DT_NEEDED — see MpvRuntime.h) once the first
    // frame is on screen, so its map/relocate cost lands in UI idle time
    // instead of the launch path. frameSwapped fires on the render thread;
    // queue onto the GUI thread and only ever start one loader.
    QObject::connect(
        &window, &QQuickWindow::frameSwapped, &window,
        [] {
            static bool started = false;
            if (started)
                return;
            started = true;
            logLine("startup: first frame swapped, preloading libmpv");
            JellyfinNative::MpvRuntime::preloadAsync();
        },
        Qt::QueuedConnection);

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
    if (HLunaServiceCall(
            "luna://com.webos.service.ime/registerRemoteKeyboard", "{\"subscribe\":true}", imeContext.get())) {
        logLine("HLunaServiceCall registerRemoteKeyboard failed (non-fatal)");
        imeContext.reset();
    } else {
        logLine("HLunaServiceCall registerRemoteKeyboard OK");
    }

    std::unique_ptr<HContext> memoryContext(new HContext());
    memoryContext->pub = true;
    memoryContext->multiple = true;
    memoryContext->callback = &lunaMemoryStatusCallback;
    memoryContext->userdata = nullptr;
    if (HLunaServiceCall(
            "luna://com.webos.service.memorymanager/getMemoryStatus", "{\"subscribe\":true}", memoryContext.get())) {
        logLine("HLunaServiceCall getMemoryStatus failed (non-fatal)");
        memoryContext.reset();
    } else {
        logLine("HLunaServiceCall getMemoryStatus OK");
    }

    std::unique_ptr<HContext> soundOutputContext(new HContext());
    soundOutputContext->pub = true;
    soundOutputContext->multiple = true;
    soundOutputContext->callback = &lunaSoundOutputCallback;
    soundOutputContext->userdata = nullptr;
    if (HLunaServiceCall(
            "luna://com.webos.service.audio/getSoundOutput", "{\"subscribe\":true}", soundOutputContext.get())) {
        logLine("HLunaServiceCall getSoundOutput failed (non-fatal)");
        soundOutputContext.reset();
    } else {
        logLine("HLunaServiceCall getSoundOutput OK");
    }

    QTimer soundOutputPollTimer;
    soundOutputPollTimer.setInterval(100);
    QObject::connect(&soundOutputPollTimer, &QTimer::timeout, controller->settings(),
        [settings = controller->settings(), appliedGeneration = std::uint64_t { 0 }]() mutable {
            const std::uint64_t eventGeneration = g_soundOutputEventGeneration.load();
            if (eventGeneration == appliedGeneration)
                return;

            QString output;
            int displayLatencyMs = -1;
            int outputLatencyMs = -1;
            {
                const std::lock_guard lock(g_pendingSoundOutputMutex);
                output = g_pendingSoundOutput;
                displayLatencyMs = g_pendingDisplayLatencyMs;
                outputLatencyMs = g_pendingOutputLatencyMs;
            }
            appliedGeneration = eventGeneration;
            if (output.isEmpty())
                return;

            settings->updateWebOSAudioOutput(output, displayLatencyMs, outputLatencyMs);
            QTimer::singleShot(250, settings, [settings, output, eventGeneration]() {
                if (g_soundOutputEventGeneration.load() != eventGeneration)
                    return;
                const WebOSAudioLatencySnapshot settledLatency = readWebOSAudioLatency();
                logLine("[ls2-audio] settled soundOutput=%s adecLipsyncMs=%d outputLatencyMs=%d", qPrintable(output),
                    settledLatency.displayLatencyMs, settledLatency.outputLatencyMs);
                settings->updateWebOSAudioOutput(
                    output, settledLatency.displayLatencyMs, settledLatency.outputLatencyMs);
            });
        });
    soundOutputPollTimer.start();
#endif

    QTimer::singleShot(0, controller.get(), &JellyfinNative::AppController::initialize);
    QTimer::singleShot(0, &window, [&window, &startupTimer]() {
        logLine(
            "startup: event loop entered, first-frame path at %lld ms", static_cast<long long>(startupTimer.elapsed()));
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
    JellyfinNative::Diagnostics::setInstanceState(
        QStringLiteral("app_exec_returned"), { { QStringLiteral("exitCode"), exitCode } });
    JellyfinNative::Diagnostics::shutdown();
    return exitCode;
}

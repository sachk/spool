#include "Diagnostics.h"

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QMutex>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#ifdef Q_OS_UNIX
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace JellyfinNative::Diagnostics {

namespace {

#ifdef JELLYFIN_DIAGNOSTICS
constexpr bool kDiagnosticsEnabled = true;
#else
constexpr bool kDiagnosticsEnabled = false;
#endif

#ifdef JELLYFIN_DIAGNOSTICS_STACKDUMP
constexpr bool kStackDumpEnabled = true;
#else
constexpr bool kStackDumpEnabled = false;
#endif

#ifdef JELLYFIN_DIAGNOSTICS_ABORT_ON_HANG
constexpr bool kAbortOnHang = true;
#else
constexpr bool kAbortOnHang = false;
#endif

constexpr qint64 kGuiWarnMs = 4000;
constexpr qint64 kShutdownWarnMs = 6000;

struct State
{
    QString appId;
    QString root;
    QString instanceId;
    QElapsedTimer uptime;
    QMutex mutex;
    std::atomic<qint64> guiHeartbeatMs { 0 };
    std::atomic_bool shuttingDown { false };
    std::atomic<qint64> shutdownStartedMs { 0 };
    std::atomic_bool watchdogRunning { false };
    std::thread watchdogThread;
};

State &state()
{
    static State s;
    return s;
}

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

qint64 uptimeMs()
{
    auto &s = state();
    return s.uptime.isValid() ? s.uptime.elapsed() : 0;
}

QString timestamp()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString procRoot()
{
    return QStringLiteral("/proc/%1").arg(QCoreApplication::applicationPid());
}

void ensureDir(const QString &path)
{
    QDir().mkpath(path);
}

void rotateFile(const QString &path, int keep)
{
    if (!QFile::exists(path))
        return;
    QFileInfo info(path);
    if (info.size() < 1024 * 1024)
        return;
    for (int i = keep - 1; i >= 1; --i) {
        const QString from = QStringLiteral("%1.%2").arg(path).arg(i);
        const QString to = QStringLiteral("%1.%2").arg(path).arg(i + 1);
        if (QFile::exists(from)) {
            QFile::remove(to);
            QFile::rename(from, to);
        }
    }
    QFile::remove(path + QStringLiteral(".1"));
    QFile::rename(path, path + QStringLiteral(".1"));
}

QString jsonPath(const QString &name)
{
    return state().root + QLatin1Char('/') + name;
}

QJsonObject baseObject(const QString &category, const QString &event)
{
    QJsonObject object;
    object.insert(QStringLiteral("ts"), timestamp());
    object.insert(QStringLiteral("uptimeMs"), uptimeMs());
    object.insert(QStringLiteral("pid"), QCoreApplication::applicationPid());
    object.insert(QStringLiteral("instanceId"), state().instanceId);
    object.insert(QStringLiteral("category"), category);
    object.insert(QStringLiteral("event"), event);
    return object;
}

QString sanitizedUrl(QString url)
{
    QRegularExpression re(QStringLiteral("([?&](?:api_key|access_token|token)=)[^&]+"), QRegularExpression::CaseInsensitiveOption);
    url.replace(re, QStringLiteral("\\1<redacted>"));
    return url;
}

void writeJsonFile(const QString &path, const QJsonObject &object)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QByteArray readSmallFile(const QString &path, qsizetype maxBytes = 128 * 1024)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.read(maxBytes);
}

QJsonObject procSnapshotObject(qint64 pid)
{
    QJsonObject object;
    const QString root = QStringLiteral("/proc/%1").arg(pid);
    object.insert(QStringLiteral("pid"), pid);
    object.insert(QStringLiteral("cmdline"), QString::fromLocal8Bit(readSmallFile(root + QStringLiteral("/cmdline"))).replace(QLatin1Char('\0'), QLatin1Char(' ')).trimmed());
    object.insert(QStringLiteral("status"), QString::fromLocal8Bit(readSmallFile(root + QStringLiteral("/status"))));
    object.insert(QStringLiteral("wchan"), QString::fromLocal8Bit(readSmallFile(root + QStringLiteral("/wchan"))).trimmed());
    object.insert(QStringLiteral("stat"), QString::fromLocal8Bit(readSmallFile(root + QStringLiteral("/stat"))).trimmed());
    object.insert(QStringLiteral("stack"), QString::fromLocal8Bit(readSmallFile(root + QStringLiteral("/stack"), 64 * 1024)));
    return object;
}

QJsonArray findMatchingProcesses()
{
    QJsonArray processes;
    const QString appId = state().appId;
    const qint64 self = QCoreApplication::applicationPid();
    QDir proc(QStringLiteral("/proc"));
    const auto entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool ok = false;
        const qint64 pid = entry.toLongLong(&ok);
        if (!ok || pid == self)
            continue;
        const QString cmdline = QString::fromLocal8Bit(readSmallFile(QStringLiteral("/proc/%1/cmdline").arg(pid))).replace(QLatin1Char('\0'), QLatin1Char(' '));
        if (!cmdline.contains(appId) && !cmdline.contains(QStringLiteral("jellyfin-native")))
            continue;
        processes.append(procSnapshotObject(pid));
    }
    return processes;
}

void writeInstance(const QString &stateName, QJsonObject extra)
{
    if (!kDiagnosticsEnabled)
        return;
    QJsonObject object;
    object.insert(QStringLiteral("instanceId"), state().instanceId);
    object.insert(QStringLiteral("appId"), state().appId);
    object.insert(QStringLiteral("pid"), QCoreApplication::applicationPid());
    object.insert(QStringLiteral("state"), stateName);
    object.insert(QStringLiteral("ts"), timestamp());
    object.insert(QStringLiteral("uptimeMs"), uptimeMs());
    object.insert(QStringLiteral("diagnosticsRoot"), state().root);
    for (auto it = extra.begin(); it != extra.end(); ++it)
        object.insert(it.key(), it.value());
    writeJsonFile(jsonPath(QStringLiteral("current-instance.json")), object);
}

void runWatchdog()
{
    auto &s = state();
    bool guiWarned = false;
    bool shutdownWarned = false;
    while (s.watchdogRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        const qint64 lastGui = s.guiHeartbeatMs.load();
        const qint64 age = lastGui > 0 ? nowMs() - lastGui : 0;
        if (age > kGuiWarnMs && !guiWarned) {
            guiWarned = true;
            logEvent(QStringLiteral("watchdog"), QStringLiteral("gui_stall"), {{QStringLiteral("ageMs"), age}});
            dumpDiagnostics(QStringLiteral("gui-stall"));
        } else if (age <= kGuiWarnMs) {
            guiWarned = false;
        }

        if (s.shuttingDown.load()) {
            const qint64 shutdownAge = nowMs() - s.shutdownStartedMs.load();
            if (shutdownAge > kShutdownWarnMs && !shutdownWarned) {
                shutdownWarned = true;
                logEvent(QStringLiteral("watchdog"), QStringLiteral("shutdown_stall"), {{QStringLiteral("ageMs"), shutdownAge}});
                dumpDiagnostics(QStringLiteral("shutdown-stall"));
                if (kAbortOnHang)
                    abort();
            }
        }
    }
}

} // namespace

void initialize(const QString &appId, const QString &rootPath)
{
    if (!kDiagnosticsEnabled)
        return;
    auto &s = state();
    QMutexLocker locker(&s.mutex);
    if (s.uptime.isValid())
        return;
    s.appId = appId;
    s.root = rootPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/diagnostics")
                                : rootPath;
    if (s.root.isEmpty())
        s.root = QStringLiteral("/tmp/%1-diagnostics").arg(appId);
    ensureDir(s.root);
    ensureDir(s.root + QStringLiteral("/proc"));
    ensureDir(s.root + QStringLiteral("/stackdump"));
    ensureDir(s.root + QStringLiteral("/watchdog"));
    rotateFile(jsonPath(QStringLiteral("lifecycle.jsonl")), 4);
    rotateFile(s.root + QStringLiteral("/watchdog/watchdog.jsonl"), 4);
    s.instanceId = QStringLiteral("%1-%2").arg(QCoreApplication::applicationPid()).arg(nowMs());
    s.uptime.start();
    s.guiHeartbeatMs = nowMs();
    writePreviousInstanceReport();
    setInstanceState(QStringLiteral("starting"));
    logEvent(QStringLiteral("lifecycle"), QStringLiteral("diagnostics_started"), {{QStringLiteral("root"), s.root}});
    s.watchdogRunning = true;
    s.watchdogThread = std::thread(runWatchdog);
}

void shutdown()
{
    if (!kDiagnosticsEnabled || !state().uptime.isValid())
        return;
    setInstanceState(QStringLiteral("exited"));
    logEvent(QStringLiteral("lifecycle"), QStringLiteral("diagnostics_stopping"));
    auto &s = state();
    s.watchdogRunning = false;
    if (s.watchdogThread.joinable())
        s.watchdogThread.join();
}

bool enabled()
{
    return kDiagnosticsEnabled;
}

QString rootPath()
{
    return state().root;
}

void logEvent(const QString &category, const QString &event, QJsonObject data)
{
    if (!kDiagnosticsEnabled || state().root.isEmpty())
        return;
    QJsonObject object = baseObject(category, event);
    for (auto it = data.begin(); it != data.end(); ++it)
        object.insert(it.key(), it.value());
    const QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    const QString path = category == QStringLiteral("watchdog")
                             ? state().root + QStringLiteral("/watchdog/watchdog.jsonl")
                             : jsonPath(QStringLiteral("lifecycle.jsonl"));
    QMutexLocker locker(&state().mutex);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append))
        file.write(line);
}

void setInstanceState(const QString &stateName, QJsonObject extra)
{
    if (!kDiagnosticsEnabled)
        return;
    if (stateName == QStringLiteral("shutting_down")) {
        state().shuttingDown = true;
        state().shutdownStartedMs = nowMs();
    }
    writeInstance(stateName, std::move(extra));
    logEvent(QStringLiteral("lifecycle"), QStringLiteral("instance_state"), {{QStringLiteral("state"), stateName}});
}

void writePreviousInstanceReport()
{
    if (!kDiagnosticsEnabled)
        return;
    QJsonObject object;
    object.insert(QStringLiteral("ts"), timestamp());
    object.insert(QStringLiteral("pid"), QCoreApplication::applicationPid());
    object.insert(QStringLiteral("processes"), findMatchingProcesses());
    const QByteArray previous = readSmallFile(jsonPath(QStringLiteral("current-instance.json")));
    if (!previous.isEmpty())
        object.insert(QStringLiteral("previousCurrentInstance"), QJsonDocument::fromJson(previous).object());
    writeJsonFile(jsonPath(QStringLiteral("stale-processes.json")), object);
    logEvent(QStringLiteral("lifecycle"), QStringLiteral("previous_instance_scan"), {{QStringLiteral("matches"), object.value(QStringLiteral("processes")).toArray().size()}});
}

void dumpDiagnostics(const QString &reason)
{
    if (!kDiagnosticsEnabled || state().root.isEmpty())
        return;
    const QString safeReason = reason.isEmpty() ? QStringLiteral("manual") : reason;
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-hhmmsszzz"));
    const QString procPath = state().root + QStringLiteral("/proc/%1-%2.json").arg(stamp, safeReason);
    QJsonObject proc;
    proc.insert(QStringLiteral("reason"), safeReason);
    proc.insert(QStringLiteral("self"), procSnapshotObject(QCoreApplication::applicationPid()));
    proc.insert(QStringLiteral("matchingProcesses"), findMatchingProcesses());
    writeJsonFile(procPath, proc);
    writePreviousInstanceReport();
    logEvent(QStringLiteral("watchdog"), QStringLiteral("proc_snapshot"), {{QStringLiteral("reason"), safeReason}, {QStringLiteral("path"), procPath}});

    if (kStackDumpEnabled) {
#ifdef Q_OS_UNIX
        const QString output = state().root + QStringLiteral("/stackdump/%1-%2.gdb.txt").arg(stamp, safeReason);
        QStringList args;
        args << QStringLiteral("-batch")
             << QStringLiteral("-ex") << QStringLiteral("set pagination off")
             << QStringLiteral("-ex") << QStringLiteral("thread apply all bt full")
             << QStringLiteral("-p") << QString::number(QCoreApplication::applicationPid());
        QProcess::startDetached(QStringLiteral("sh"), {QStringLiteral("-c"), QStringLiteral("gdb %1 > %2 2>&1").arg(args.join(QLatin1Char(' ')), output)});
        logEvent(QStringLiteral("watchdog"), QStringLiteral("stackdump_requested"), {{QStringLiteral("path"), output}});
#endif
    }
}

void noteSignal(int signalNumber)
{
    logEvent(QStringLiteral("signal"), QStringLiteral("received"), {{QStringLiteral("signal"), signalNumber}});
}

EventLoopWatchdog::EventLoopWatchdog(QObject *parent)
    : QObject(parent)
{
    if (!kDiagnosticsEnabled)
        return;
    state().guiHeartbeatMs = nowMs();
    if (auto *dispatcher = QAbstractEventDispatcher::instance(QThread::currentThread())) {
        connect(dispatcher, &QAbstractEventDispatcher::awake, this, []() { state().guiHeartbeatMs = nowMs(); });
        connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, []() { state().guiHeartbeatMs = nowMs(); });
    }
    auto *timer = new QTimer(this);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, this, []() { state().guiHeartbeatMs = nowMs(); });
    timer->start();
    logEvent(QStringLiteral("watchdog"), QStringLiteral("event_loop_watchdog_started"));
}

EventLoopWatchdog::~EventLoopWatchdog()
{
    logEvent(QStringLiteral("watchdog"), QStringLiteral("event_loop_watchdog_stopped"));
}

Phase::Phase(QString category, QString name, QJsonObject data)
    : m_category(std::move(category)), m_name(std::move(name)), m_startedMs(nowMs()), m_active(kDiagnosticsEnabled)
{
    if (m_active) {
        data.insert(QStringLiteral("phase"), m_name);
        logEvent(m_category, QStringLiteral("phase_begin"), data);
    }
}

Phase::~Phase()
{
    if (m_active)
        logEvent(m_category, QStringLiteral("phase_end"), {{QStringLiteral("phase"), m_name}, {QStringLiteral("durationMs"), nowMs() - m_startedMs}});
}

Task::Task(QString name, QJsonObject data)
    : m_name(std::move(name)), m_id(QStringLiteral("task-%1-%2").arg(QCoreApplication::applicationPid()).arg(nowMs())), m_startedMs(nowMs()), m_active(kDiagnosticsEnabled)
{
    if (m_active) {
        data.insert(QStringLiteral("task"), m_name);
        data.insert(QStringLiteral("taskId"), m_id);
        logEvent(QStringLiteral("task"), QStringLiteral("begin"), data);
    }
}

Task::~Task()
{
    if (m_active)
        logEvent(QStringLiteral("task"), QStringLiteral("end"), {{QStringLiteral("task"), m_name}, {QStringLiteral("taskId"), m_id}, {QStringLiteral("durationMs"), nowMs() - m_startedMs}});
}

ThreadScope::ThreadScope(QString name)
    : m_name(std::move(name)), m_active(kDiagnosticsEnabled)
{
    if (m_active)
        logEvent(QStringLiteral("thread"), QStringLiteral("registered"), {{QStringLiteral("name"), m_name}, {QStringLiteral("qtThread"), QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16)}});
}

ThreadScope::~ThreadScope()
{
    if (m_active)
        logEvent(QStringLiteral("thread"), QStringLiteral("unregistered"), {{QStringLiteral("name"), m_name}});
}

NetworkRequest::NetworkRequest(QString method, QString url)
    : m_id(QStringLiteral("net-%1-%2").arg(QCoreApplication::applicationPid()).arg(nowMs())), m_method(std::move(method)), m_url(sanitizedUrl(std::move(url))), m_startedMs(nowMs())
{
    logEvent(QStringLiteral("network"), QStringLiteral("request_begin"), {{QStringLiteral("requestId"), m_id}, {QStringLiteral("method"), m_method}, {QStringLiteral("url"), m_url}});
}

NetworkRequest::~NetworkRequest()
{
    if (!m_finished)
        finish(0, QStringLiteral("unfinished"));
}

void NetworkRequest::finish(int statusCode, const QString &errorText)
{
    if (m_finished)
        return;
    m_finished = true;
    logEvent(QStringLiteral("network"), QStringLiteral("request_end"), {{QStringLiteral("requestId"), m_id}, {QStringLiteral("method"), m_method}, {QStringLiteral("url"), m_url}, {QStringLiteral("statusCode"), statusCode}, {QStringLiteral("error"), errorText}, {QStringLiteral("durationMs"), nowMs() - m_startedMs}});
}

} // namespace JellyfinNative::Diagnostics

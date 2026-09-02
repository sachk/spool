#include "RenderBenchmark.h"

#include "../app/AppController.h"
#include "../app/RouterController.h"
#include "InputLatencyMonitor.h"

#include <QChronoTimer>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QTimer>
#include <QtGlobal>

#include <algorithm>
#include <chrono>

namespace JellyfinNative {
namespace {

    // Routes that stand up without a library behind them, so the same script runs
    // on a developer's machine against a real server and in CI against nothing.
    const QStringList kDefaultScript
        = { QStringLiteral("home"), QStringLiteral("search"), QStringLiteral("settings"), QStringLiteral("home"),
              QStringLiteral("libraryGrid"), QStringLiteral("openSourceNotices"), QStringLiteral("home") };

    int envInt(const char *name, int fallback)
    {
        bool ok = false;
        const int value = qEnvironmentVariableIntValue(name, &ok);
        return ok ? value : fallback;
    }

} // namespace

RenderBenchmark *RenderBenchmark::createIfRequested(
    AppController *app, RouterController *router, InputLatencyMonitor *latency, QQuickWindow *window, QObject *parent)
{
    const QString script = qEnvironmentVariable("SPOOL_BENCH").trimmed();
    if (script.isEmpty())
        return nullptr;
    auto *benchmark = new RenderBenchmark(app, router, latency, window, parent);
    if (script != QStringLiteral("routes"))
        benchmark->m_script = script.split(QLatin1Char(','), Qt::SkipEmptyParts);
    return benchmark;
}

RenderBenchmark::RenderBenchmark(
    AppController *app, RouterController *router, InputLatencyMonitor *latency, QQuickWindow *window, QObject *parent)
    : QObject(parent)
    , m_app(app)
    , m_router(router)
    , m_latency(latency)
    , m_window(window)
    , m_script(kDefaultScript)
    , m_outputPath(qEnvironmentVariable("SPOOL_BENCH_OUT"))
    , m_iterations(envInt("SPOOL_BENCH_ITERATIONS", 3))
    , m_warmup(envInt("SPOOL_BENCH_WARMUP", 1))
    , m_stepTimeoutMs(envInt("SPOOL_BENCH_TIMEOUT_MS", 8000))
    , m_settleMs(envInt("SPOOL_BENCH_SETTLE_MS", 120))
    , m_forceCold(qEnvironmentVariableIntValue("SPOOL_BENCH_COLD") != 0)
{
    // Every route switch ends by publishing what it cost. That is the signal
    // the walk advances on, so the harness measures the same transition the
    // diagnostics overlay shows rather than a stopwatch of its own.
    connect(m_latency, &InputLatencyMonitor::routeSampleChanged, this, [this] {
        if (m_awaitingSample)
            recordSample();
    });

    m_pump = new QTimer(this);
    m_pump->setInterval(8);
    connect(m_pump, &QTimer::timeout, this, [this] {
        if (m_window)
            m_window->requestUpdate();
    });

    m_idleProbe = new QChronoTimer(this);
    connect(m_idleProbe, &QChronoTimer::timeout, this, [this] {
        const qint64 nowNs = m_idleClock.nsecsElapsed();
        m_idleWorstNs = std::max(m_idleWorstNs, std::max<qint64>(0, nowNs - m_idleExpectedNs));
        m_idleExpectedNs = nowNs + budgetNs();
    });
}

qint64 RenderBenchmark::budgetNs() const
{
    const double budgetMs = m_latency ? m_latency->frameBudgetMs() : 16.67;
    return static_cast<qint64>((budgetMs > 0.0 ? budgetMs : 16.67) * 1000000.0);
}

void RenderBenchmark::beginIdleProbe()
{
    m_idleWorstNs = 0;
    m_idleClock.start();
    m_idleExpectedNs = budgetNs();
    m_idleProbe->setInterval(std::chrono::nanoseconds(budgetNs()));
    m_idleProbe->start();
}

void RenderBenchmark::finishIdleProbe()
{
    if (!m_idleProbe->isActive())
        return;
    m_idleProbe->stop();
    // Warmup passes are not recorded, and neither is their noise: the two
    // have to describe the same stretch of the run to be comparable.
    if (m_pass >= 0)
        m_idleGaps.append(static_cast<double>(m_idleWorstNs) / 1000000.0);
}

void RenderBenchmark::start()
{
    if (m_script.isEmpty()) {
        finish();
        return;
    }
    // The timeline is off unless somebody asked for it, which is right for a
    // normal run and wrong here: measuring is the entire job.
    m_latency->setEnabled(true);
    qInfo() << "render benchmark: script" << m_script << "iterations" << m_iterations << "warmup" << m_warmup;
    // Measurement refuses to run against a window nobody can see, which is
    // easy to arrive at by accident on a headless machine. Say so plainly
    // rather than producing an empty report.
    if (m_window) {
        qInfo() << "render benchmark: window visible=" << m_window->isVisible() << "exposed=" << m_window->isExposed();
    }
    // One pass before the first recorded one, so a cold cache and a
    // just-in-time compile do not get counted as the steady state.
    m_pass = -m_warmup;
    m_position = -1;
    QTimer::singleShot(0, this, [this] { step(); });
}

void RenderBenchmark::step()
{
    ++m_position;
    if (m_position >= m_script.size()) {
        m_position = 0;
        if (++m_pass >= m_iterations) {
            finish();
            return;
        }
    }

    const QString route = m_script.at(m_position);
    // Replacing the route you are already on is not a transition, so nothing
    // would report and the walk would sit waiting for a frame that is never
    // coming. Step over it rather than time out.
    if (m_router->route() == route) {
        QTimer::singleShot(0, this, [this] { step(); });
        return;
    }

    if (m_forceCold)
        m_app->onMemoryPressure(QStringLiteral("critical"));

    m_awaitingSample = true;
    m_stepTimer.start();
    m_pump->start();
    m_router->replace(route);
    if (m_window)
        m_window->requestUpdate();

    // A route that never reports is a failure worth seeing rather than a
    // hang: give up on it and carry on with the rest of the walk.
    QTimer::singleShot(m_stepTimeoutMs, this, [this, route] {
        if (!m_awaitingSample)
            return;
        qWarning() << "render benchmark: route" << route << "never reported a frame";
        m_awaitingSample = false;
        m_pump->stop();
        QTimer::singleShot(0, this, [this] { step(); });
    });
}

void RenderBenchmark::recordSample()
{
    m_awaitingSample = false;
    m_pump->stop();
    QVariantMap metrics = m_latency->lastRouteMetrics();
    if (!metrics.isEmpty() && m_pass >= 0) {
        metrics.insert(QStringLiteral("pass"), m_pass);
        metrics.insert(QStringLiteral("step"), m_position);
        m_samples.append(metrics);
    }
    // Let the frame settle before asking for the next route, so one
    // transition's tail is not charged to the next one's head. The app has
    // nothing to do in that window, which is exactly when the machine's own
    // scheduling jitter can be measured.
    beginIdleProbe();
    QTimer::singleShot(m_settleMs, this, [this] {
        finishIdleProbe();
        step();
    });
}

void RenderBenchmark::finish()
{
    QJsonArray samples;
    for (const QVariant& sample : std::as_const(m_samples))
        samples.append(QJsonObject::fromVariantMap(sample.toMap()));

    QJsonObject report;
    report.insert(QStringLiteral("recordedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    report.insert(QStringLiteral("script"), QJsonArray::fromStringList(m_script));
    report.insert(QStringLiteral("iterations"), m_iterations);
    report.insert(QStringLiteral("cold"), m_forceCold);
    report.insert(QStringLiteral("frameBudgetMs"), m_latency ? m_latency->frameBudgetMs() : 16.67);
    // Which paint path produced these numbers. It decides whether the frame
    // gaps mean anything: the software backend rasterises on the CPU, so on a
    // small runner a gap is mostly llvmpipe's throughput rather than anything
    // this application did.
    report.insert(QStringLiteral("quickBackend"), qEnvironmentVariable("QT_QUICK_BACKEND"));
    // What a frame-budget timer drifted by while the app was doing nothing.
    // A transition gap only means something when it is worse than this.
    QJsonArray idleGaps;
    for (const QVariant& gap : std::as_const(m_idleGaps))
        idleGaps.append(gap.toDouble());
    report.insert(QStringLiteral("idleGapsMs"), idleGaps);
    report.insert(QStringLiteral("samples"), samples);

    const QByteArray json = QJsonDocument(report).toJson(QJsonDocument::Indented);
    if (m_outputPath.isEmpty()) {
        qInfo().noquote() << json;
    } else {
        QDir().mkpath(QFileInfo(m_outputPath).absolutePath());
        QFile file(m_outputPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(json);
            qInfo() << "render benchmark: wrote" << m_samples.size() << "samples to" << m_outputPath;
        } else {
            qWarning() << "render benchmark: could not write" << m_outputPath;
        }
    }
    QCoreApplication::exit(m_samples.isEmpty() ? 1 : 0);
}

} // namespace JellyfinNative

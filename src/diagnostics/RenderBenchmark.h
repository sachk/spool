#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class QQuickWindow;

namespace JellyfinNative {
class InputLatencyMonitor;
class RouterController;
class AppController;

// Walks the app through a scripted set of route switches and writes down what
// each one cost, so "does a page still appear in one frame" is a number in CI
// rather than an impression on somebody's desk.
//
// It drives the real shell -- real models, real artwork, real delegates --
// because the costs worth watching are the ones that only appear when all of
// that is present. Without a server it still measures what a route costs to
// build and paint, which is the part that regresses when a page gains a
// binding it did not need.
//
// Off unless SPOOL_BENCH names a script, so it is inert in a normal run.
class RenderBenchmark final : public QObject {
    Q_OBJECT

public:
    // Returns nullptr when SPOOL_BENCH is unset, which is every ordinary run.
    static RenderBenchmark *createIfRequested(AppController *app, RouterController *router,
        InputLatencyMonitor *latency, QQuickWindow *window, QObject *parent);

    void start();

private:
    RenderBenchmark(AppController *app, RouterController *router, InputLatencyMonitor *latency, QQuickWindow *window,
        QObject *parent);

    void step();
    void recordSample();
    void finish();

    AppController *m_app = nullptr;
    RouterController *m_router = nullptr;
    InputLatencyMonitor *m_latency = nullptr;
    // Offscreen, nothing asks for a frame on its own: no compositor is
    // driving the window and an incubating page paints nothing until it is
    // finished. A transition is only recorded once a frame carrying it has
    // been swapped, so the harness has to keep asking for one.
    QQuickWindow *m_window = nullptr;
    QTimer *m_pump = nullptr;

    QStringList m_script;
    QString m_outputPath;
    int m_iterations = 3;
    int m_warmup = 1;
    int m_stepTimeoutMs = 8000;
    // Drop every cached page before each step, so the walk measures what a
    // route costs to build rather than what it costs to reveal. This is the
    // case a television lives in, where memory keeps nothing resident.
    bool m_forceCold = false;

    int m_position = -1;
    int m_pass = 0;
    bool m_awaitingSample = false;
    QVariantList m_samples;
    QElapsedTimer m_stepTimer;
};

} // namespace JellyfinNative

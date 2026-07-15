#pragma once

#include <QChronoTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>

class QEvent;
class QQuickWindow;
class QScreen;

namespace JellyfinNative {

namespace Detail {

    enum class InputLatencyEventKind : quint8 {
        KeyPress,
        KeyRelease,
        MousePress,
        MouseRelease,
        MouseDoubleClick,
        MouseMove,
        Wheel,
        TouchBegin,
        TouchUpdate,
        TouchEnd,
        TouchCancel,
        TabletPress,
        TabletMove,
        TabletRelease,
    };

    enum class InputLatencyStage : quint8 {
        None,
        WaitingForSync,
        Synchronizing,
        Rendering,
        Submitted,
        PresentQueued,
    };

    enum class InputLatencyRefreshSource : quint8 {
        Fallback60,
        Screen,
    };
    enum class InputLatencyRecordSlot : quint8 {
        Pending,
        Current,
    };

    struct InputLatencyEventMetadata {
        InputLatencyEventKind kind = InputLatencyEventKind::KeyPress;
        int key = 0;
        quint32 nativeScanCode = 0;
    };

    struct InputLatencySample {
        quint64 sequence = 0;
        quint64 epoch = 0;
        InputLatencyRecordSlot recordSlot = InputLatencyRecordSlot::Pending;
        InputLatencyEventMetadata event;
        quint64 coalesced = 0;
        qint64 budgetNs = 0;
        InputLatencyRefreshSource refreshSource = InputLatencyRefreshSource::Fallback60;
        qint64 totalNs = 0;
        qint64 dispatchNs = -1;
        qint64 syncBeginNs = -1;
        qint64 syncEndNs = -1;
        qint64 renderEndNs = -1;
        qint64 frameEndNs = -1;
        qint64 presentQueueNs = -1;
        InputLatencyStage stage = InputLatencyStage::None;
        bool late = false;
        bool exposed = false;
    };

    struct InputLatencyExpiredSamples {
        std::array<InputLatencySample, 2> samples;
        std::size_t count = 0;
    };
    struct UiLatencySample {
        QString name;
        qint64 budgetNs = 0;
        qint64 totalNs = 0;
        qint64 loadNs = 0;
        qint64 presentNs = 0;
        quint64 frames = 0;
    };

    std::optional<InputLatencyEventMetadata> classifyInputEvent(const QEvent *event, bool spontaneous);
    QString inputLatencyEventName(InputLatencyEventKind kind);
    QString inputLatencyStageName(InputLatencyStage stage);
    QString formatInputLatencyMiss(const InputLatencySample& sample);
    QString formatUiLatency(const UiLatencySample& sample);

    class InputLatencyTimeline final {
    public:
        using Nanoseconds = std::chrono::nanoseconds;

        bool enabled() const;
        void setEnabled(bool enabled);

        quint64 beginInput(const InputLatencyEventMetadata& event, Nanoseconds now, Nanoseconds budget,
            InputLatencyRefreshSource refreshSource, bool exposed);
        void endInput(quint64 token, Nanoseconds now);

        void beforeSynchronizing(Nanoseconds now);
        void afterSynchronizing(Nanoseconds now);
        void afterRendering(Nanoseconds now);
        void frameSwapped(Nanoseconds now);
        std::optional<InputLatencySample> afterFrameEnd(Nanoseconds now, bool exposed);

        InputLatencyExpiredSamples expire(Nanoseconds now, bool exposed);
        std::optional<Nanoseconds> nextDeadline() const;

        void cancel();
        void clearStatistics();
        bool publish(const InputLatencySample& sample);

        quint64 sampleCount() const;
        quint64 lateCount() const;
        double lastLatencyMs() const;
        quint64 missedFrameCount() const;
        double worstLatencyMs() const;
        QString lastStage() const;
        quint64 epoch() const;

    private:
        struct PendingRecord {
            bool active = false;
            bool finalized = false;
            quint64 sequence = 0;
            quint64 epoch = 0;
            quint64 firstToken = 0;
            InputLatencyEventMetadata event;
            quint64 coalesced = 0;
            qint64 inputNs = 0;
            qint64 deadlineNs = 0;
            qint64 budgetNs = 0;
            InputLatencyRefreshSource refreshSource = InputLatencyRefreshSource::Fallback60;
            qint64 dispatchNs = -1;
            bool exposed = false;
        };

        struct RenderRecord {
            std::atomic<InputLatencyStage> stage { InputLatencyStage::None };
            std::atomic_bool finalized { false };
            quint64 sequence = 0;
            quint64 epoch = 0;
            InputLatencyEventMetadata event;
            quint64 coalesced = 0;
            qint64 inputNs = 0;
            qint64 deadlineNs = 0;
            qint64 budgetNs = 0;
            InputLatencyRefreshSource refreshSource = InputLatencyRefreshSource::Fallback60;
            qint64 dispatchNs = -1;
            qint64 syncBeginNs = -1;
            qint64 syncEndNs = -1;
            qint64 renderEndNs = -1;
            qint64 frameEndNs = -1;
            qint64 presentQueueNs = -1;
            bool exposed = false;
        };

        InputLatencySample sampleFromPending(qint64 terminalNs) const;
        InputLatencySample sampleFromCurrent(
            qint64 terminalNs, InputLatencyStage publishedStage, bool completedFrame) const;
        void resetCurrentFromPending(qint64 syncBeginNs);
        bool currentMatchesEpoch(InputLatencyStage stage) const;

        std::atomic<quint64> m_epoch { 1 };
        bool m_enabled = false;
        quint64 m_nextToken = 0;
        quint64 m_nextSequence = 0;
        PendingRecord m_pending;
        RenderRecord m_current;
        std::array<std::atomic<quint64>, 2> m_publicationPermits {};

        quint64 m_sampleCount = 0;
        quint64 m_lateCount = 0;
        quint64 m_missedFrameCount = 0;
        qint64 m_lastLatencyNs = 0;
        qint64 m_worstLatencyNs = 0;
        InputLatencyStage m_lastStage = InputLatencyStage::None;
    };

} // namespace Detail

class InputLatencyMonitor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool warningVisible READ warningVisible NOTIFY warningChanged)
    Q_PROPERTY(QString warningText READ warningText NOTIFY warningChanged)
    Q_PROPERTY(QString warningStage READ warningStage NOTIFY warningChanged)
    Q_PROPERTY(double lastLatencyMs READ lastLatencyMs NOTIFY statisticsChanged)
    Q_PROPERTY(double worstLatencyMs READ worstLatencyMs NOTIFY statisticsChanged)
    Q_PROPERTY(QString lastStage READ lastStage NOTIFY statisticsChanged)
    Q_PROPERTY(quint64 sampleCount READ sampleCount NOTIFY statisticsChanged)
    Q_PROPERTY(quint64 lateCount READ lateCount NOTIFY statisticsChanged)
    Q_PROPERTY(quint64 missedFrameCount READ missedFrameCount NOTIFY statisticsChanged)
    Q_PROPERTY(double frameBudgetMs READ frameBudgetMs NOTIFY frameBudgetChanged)

public:
    explicit InputLatencyMonitor(QObject *parent = nullptr);

    void attachWindow(QQuickWindow *window);
    quint64 beginInput(const QEvent *event);
    void endInput(quint64 token);

    bool enabled() const;
    void setEnabled(bool enabled);
    bool warningVisible() const;
    QString warningText() const;
    QString warningStage() const;
    double lastLatencyMs() const;
    double worstLatencyMs() const;
    QString lastStage() const;
    quint64 sampleCount() const;
    quint64 lateCount() const;
    quint64 missedFrameCount() const;
    double frameBudgetMs() const;

    Q_INVOKABLE void clearStatistics();
    Q_INVOKABLE quint64 beginUiTransition(const QString& name);
    Q_INVOKABLE void markUiTransitionReady(quint64 token);

signals:
    void enabledChanged();
    void warningChanged();
    void statisticsChanged();
    void frameBudgetChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static Detail::InputLatencyTimeline::Nanoseconds now();
    void updateScreen(QScreen *screen);
    void updateFrameBudget();
    void scheduleDeadline();
    void handleDeadline();
    void handleCompletedSample(const Detail::InputLatencySample& sample);
    void handleUiTransitionFrame(qint64 frameNs);
    void cancelMeasurements();
    void finishCancellationOnGuiThread();
    void hideWarning();
    bool canCaptureInput() const;

    Detail::InputLatencyTimeline m_timeline;
    QQuickWindow *m_window = nullptr;
    QScreen *m_screen = nullptr;
    QMetaObject::Connection m_refreshRateConnection;
    QChronoTimer m_deadlineTimer;
    QTimer m_warningTimer;
    qint64 m_frameBudgetNs = 16670000;
    Detail::InputLatencyRefreshSource m_refreshSource = Detail::InputLatencyRefreshSource::Fallback60;
    QString m_warningText;
    QString m_warningStage;
    qint64 m_warningLatencyNs = 0;
    bool m_warningVisible = false;
    quint64 m_uiTransitionSequence = 0;
    quint64 m_uiTransitionToken = 0;
    QString m_uiTransitionName;
    qint64 m_uiTransitionBeginNs = -1;
    qint64 m_uiTransitionReadyNs = -1;
    // Read on the render thread each frame swap; queue work to the GUI
    // thread only while a transition is actually being measured.
    std::atomic_bool m_uiTransitionActive { false };
};

} // namespace JellyfinNative

#ifndef LISTENER_SCRIPT_QUEUE_H
#define LISTENER_SCRIPT_QUEUE_H

#include "ports/OutputCapture.h"
#include "ports/RunEvent.h"
#include "ports/RunEventSink.h"
#include "ports/ScriptRunner.h"

#include <QObject>
#include <QString>
#include <deque>
#include <memory>
#include <optional>

class QTimer;

// Deep module (architecture §3.1): FIFO, single-active ScriptRunner::run, orphan→Null.
class ScriptQueue final : public QObject
{
    Q_OBJECT

public:
    // Default heartbeat interval matches architecture §6 (~15s).
    static constexpr int kDefaultHeartbeatIntervalMs = 15'000;

    ScriptQueue(ScriptRunner *runner,
                OutputCapture *capture,
                QObject *parent = nullptr);

    // SubmitRun driving port surface.
    RunHandle enqueue(const RunRequest &request);

    // Client disconnect: swap sink to Null; do not cancel host run (v1).
    void onClientDetached(const QString &jobId);

    [[nodiscard]] QString activeJobId() const;
    [[nodiscard]] int pendingCount() const;

    // Test seam: shorten heartbeat so QTimer path is exercisable.
    void setHeartbeatIntervalMs(int intervalMs);
    [[nodiscard]] int heartbeatIntervalMs() const;

private:
    struct Job {
        QString id;
        QString scriptUtf8;
        std::shared_ptr<RunEventSink> sink;
        bool detached{false};
        bool started{false};
    };

    void tryStartNext();
    void executeJob(Job job);
    void emitForJob(const Job &job, const RunEvent &event) const;
    void emitChunks(Job &job, const QVector<OutputChunk> &chunks) const;
    void onHeartbeatTick();

    ScriptRunner *runner_{nullptr};
    OutputCapture *capture_{nullptr};
    std::deque<Job> pending_;
    std::optional<Job> active_;
    int nextJobSerial_{1};
    int heartbeatIntervalMs_{kDefaultHeartbeatIntervalMs};
    QTimer *heartbeatTimer_{nullptr};
    qint64 activeStartedMs_{0};
    // Shared Null sink instance for orphaned jobs.
    std::shared_ptr<RunEventSink> nullSink_;
};

#endif // LISTENER_SCRIPT_QUEUE_H

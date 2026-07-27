#include "ScriptQueue.h"
#include "NullEventSink.h"

#include <QDateTime>
#include <QTimer>

namespace
{
QString makeIsoTimestamp()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}
} // namespace

ScriptQueue::ScriptQueue(ScriptRunner *runner, OutputCapture *capture, QObject *parent)
    : QObject(parent),
      runner_(runner),
      capture_(capture),
      nullSink_(std::make_shared<NullEventSink>())
{
    heartbeatTimer_ = new QTimer(this);
    heartbeatTimer_->setSingleShot(false);
    connect(heartbeatTimer_, &QTimer::timeout, this, &ScriptQueue::onHeartbeatTick);
}

RunHandle ScriptQueue::enqueue(const RunRequest &request)
{
    const QString id = QStringLiteral("job-%1").arg(nextJobSerial_++);
    std::shared_ptr<RunEventSink> sink;
    if (request.eventSink != nullptr) {
        // Non-owning view wrapped for lifetime managed by caller; we only
        // replace with shared Null on detach. For tests, fakes outlive the queue.
        sink = std::shared_ptr<RunEventSink>(request.eventSink,
                                             [](RunEventSink *)
                                             {
                                             });
    }
    else {
        sink = nullSink_;
    }

    Job job;
    job.id = id;
    job.scriptUtf8 = request.scriptUtf8;
    job.sink = std::move(sink);

    // position: 0 = next to run. Active job is not in pending.
    const int position = active_.has_value() ? static_cast<int>(pending_.size()) : 0;

    RunEvent queued;
    queued.type = RunEventType::Queued;
    queued.jobId = id;
    queued.position = position;
    queued.ts = makeIsoTimestamp();
    emitForJob(job, queued);

    // Empty body → immediate failed (architecture §2.4.1 request rules).
    if (job.scriptUtf8.isEmpty()) {
        RunEvent failed;
        failed.type = RunEventType::Failed;
        failed.jobId = id;
        failed.durationMs = 0;
        failed.error = QStringLiteral("empty script body");
        failed.ts = makeIsoTimestamp();
        emitForJob(job, failed);
        if (job.sink) {
            job.sink->close();
        }
        return RunHandle{.id = id, .queuePosition = position};
    }

    pending_.push_back(std::move(job));
    tryStartNext();
    return RunHandle{.id = id, .queuePosition = position};
}

void ScriptQueue::onClientDetached(const QString &jobId)
{
    if (active_.has_value() && active_->id == jobId) {
        active_->detached = true;
        active_->sink = nullSink_;
        return;
    }

    for (auto &job : pending_) {
        if (job.id == jobId) {
            job.detached = true;
            job.sink = nullSink_;
            return;
        }
    }
}

QString ScriptQueue::activeJobId() const
{
    return active_.has_value() ? active_->id : QString();
}

int ScriptQueue::pendingCount() const
{
    return static_cast<int>(pending_.size());
}

void ScriptQueue::setHeartbeatIntervalMs(const int intervalMs)
{
    heartbeatIntervalMs_ = intervalMs > 0 ? intervalMs : kDefaultHeartbeatIntervalMs;
    if (heartbeatTimer_->isActive()) {
        heartbeatTimer_->setInterval(heartbeatIntervalMs_);
    }
}

int ScriptQueue::heartbeatIntervalMs() const
{
    return heartbeatIntervalMs_;
}

void ScriptQueue::tryStartNext()
{
    if (active_.has_value()) {
        return;
    }

    while (!pending_.empty()) {
        Job job = std::move(pending_.front());
        pending_.pop_front();

        // Orphan still queued: fail without starting host run (architecture §6).
        if (job.detached) {
            RunEvent failed;
            failed.type = RunEventType::Failed;
            failed.jobId = job.id;
            failed.durationMs = 0;
            failed.error = QStringLiteral("client detached before start");
            failed.ts = makeIsoTimestamp();
            emitForJob(job, failed);
            if (job.sink) {
                job.sink->close();
            }
            continue;
        }

        executeJob(std::move(job));
        return;
    }
}

void ScriptQueue::executeJob(Job job)
{
    job.started = true;
    active_ = job;
    activeStartedMs_ = QDateTime::currentMSecsSinceEpoch();

    RunEvent started;
    started.type = RunEventType::Started;
    started.jobId = job.id;
    started.ts = makeIsoTimestamp();
    emitForJob(*active_, started);

    RunEvent logPos;
    logPos.type = RunEventType::Log;
    logPos.jobId = job.id;
    logPos.text = QStringLiteral("queue position 0");
    logPos.level = QStringLiteral("info");
    logPos.ts = makeIsoTimestamp();
    emitForJob(*active_, logPos);

    if (capture_ != nullptr) {
        capture_->start(job.id);
    }

    heartbeatTimer_->setInterval(heartbeatIntervalMs_);
    heartbeatTimer_->start();

    RunResult result{.ok = true, .errorMessage = {}};
    if (runner_ != nullptr) {
        result = runner_->run(job.scriptUtf8, job.id);
    }

    // Capture final chunks after host return (and any mid-run poll via heartbeat).
    if (capture_ != nullptr) {
        emitChunks(*active_, capture_->poll(job.id));
        emitChunks(*active_, capture_->stop(job.id));
    }

    heartbeatTimer_->stop();

    const qint64 durationMs = QDateTime::currentMSecsSinceEpoch() - activeStartedMs_;

    if (result.ok) {
        RunEvent finished;
        finished.type = RunEventType::Finished;
        finished.jobId = job.id;
        finished.durationMs = durationMs;
        finished.ok = true;
        finished.ts = makeIsoTimestamp();
        emitForJob(*active_, finished);
    }
    else {
        RunEvent failed;
        failed.type = RunEventType::Failed;
        failed.jobId = job.id;
        failed.durationMs = durationMs;
        failed.error = result.errorMessage.isEmpty()
                           ? QStringLiteral("script failed")
                           : result.errorMessage;
        failed.ts = makeIsoTimestamp();
        emitForJob(*active_, failed);
    }

    if (active_->sink) {
        active_->sink->close();
    }
    active_.reset();
    tryStartNext();
}

void ScriptQueue::emitForJob(const Job &job, const RunEvent &event) const
{
    if (job.sink) {
        job.sink->emitEvent(event);
    }
}

void ScriptQueue::emitChunks(Job &job, const QVector<OutputChunk> &chunks) const
{
    for (const OutputChunk &chunk : chunks) {
        RunEvent ev;
        ev.type = chunk.stream == OutputChunk::Stream::Stdout
                      ? RunEventType::Stdout
                      : RunEventType::Stderr;
        ev.jobId = job.id;
        ev.text = chunk.text;
        ev.ts = makeIsoTimestamp();
        emitForJob(job, ev);
    }
}

void ScriptQueue::onHeartbeatTick()
{
    if (!active_.has_value()) {
        return;
    }

    if (capture_ != nullptr) {
        emitChunks(*active_, capture_->poll(active_->id));
    }

    RunEvent hb;
    hb.type = RunEventType::Heartbeat;
    hb.jobId = active_->id;
    hb.elapsedMs = QDateTime::currentMSecsSinceEpoch() - activeStartedMs_;
    hb.ts = makeIsoTimestamp();
    emitForJob(*active_, hb);
}

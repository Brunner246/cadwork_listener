#ifndef LISTENER_FAKES_FAKE_OUTPUT_CAPTURE_H
#define LISTENER_FAKES_FAKE_OUTPUT_CAPTURE_H

#include "ports/OutputCapture.h"

#include <QHash>
#include <QVector>

// Fake OutputCapture (architecture §5): canned chunks on poll/stop.
class FakeOutputCapture final : public OutputCapture
{
public:
    QHash<QString, QVector<OutputChunk>> pollChunksByJob;
    QHash<QString, QVector<OutputChunk>> stopChunksByJob;
    QVector<QString> startedJobs;
    QVector<QString> stoppedJobs;
    int pollCount{0};

    CaptureHandles start(const QString &jobId) override
    {
        startedJobs.append(jobId);
        return CaptureHandles{jobId};
    }

    QVector<OutputChunk> poll(const QString &jobId) override
    {
        ++pollCount;
        const auto it = pollChunksByJob.constFind(jobId);
        if (it == pollChunksByJob.cend()) {
            return {};
        }
        // Emit once then clear so heartbeats do not duplicate forever.
        const QVector<OutputChunk> chunks = it.value();
        pollChunksByJob.remove(jobId);
        return chunks;
    }

    QVector<OutputChunk> stop(const QString &jobId) override
    {
        stoppedJobs.append(jobId);
        const auto it = stopChunksByJob.constFind(jobId);
        if (it == stopChunksByJob.cend()) {
            return {};
        }
        return it.value();
    }
};

#endif // LISTENER_FAKES_FAKE_OUTPUT_CAPTURE_H

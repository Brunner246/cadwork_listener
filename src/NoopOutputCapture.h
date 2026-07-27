#ifndef LISTENER_NOOP_OUTPUT_CAPTURE_H
#define LISTENER_NOOP_OUTPUT_CAPTURE_H

#include "ports/OutputCapture.h"

// Placeholder OutputCapture until FileTee (seed 04). Emits no chunks.
class NoopOutputCapture final : public OutputCapture
{
public:
    CaptureHandles start(const QString &jobId) override
    {
        return CaptureHandles{jobId};
    }

    QVector<OutputChunk> poll(const QString & /*jobId*/) override
    {
        return {};
    }

    QVector<OutputChunk> stop(const QString & /*jobId*/) override
    {
        return {};
    }
};

#endif // LISTENER_NOOP_OUTPUT_CAPTURE_H

#ifndef LISTENER_PORTS_OUTPUT_CAPTURE_H
#define LISTENER_PORTS_OUTPUT_CAPTURE_H

#include "RunEvent.h"

#include <QVector>

// Driven port (architecture §2.2): start / poll / stop only.
class OutputCapture
{
public:
    virtual ~OutputCapture() = default;

    virtual CaptureHandles start(const QString &jobId) = 0;
    virtual QVector<OutputChunk> poll(const QString &jobId) = 0;
    virtual QVector<OutputChunk> stop(const QString &jobId) = 0;
};

#endif // LISTENER_PORTS_OUTPUT_CAPTURE_H

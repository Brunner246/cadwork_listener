#ifndef LISTENER_PORTS_RUN_EVENT_SINK_H
#define LISTENER_PORTS_RUN_EVENT_SINK_H

#include "RunEvent.h"

// Driven port (architecture §2.2). Method is emitEvent because Qt's emit macro
// would clobber a method named emit.
class RunEventSink
{
public:
    virtual ~RunEventSink() = default;

    virtual void emitEvent(const RunEvent &event) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;
};

#endif // LISTENER_PORTS_RUN_EVENT_SINK_H

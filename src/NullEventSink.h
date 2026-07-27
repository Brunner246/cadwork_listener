#ifndef LISTENER_NULL_EVENT_SINK_H
#define LISTENER_NULL_EVENT_SINK_H

#include "ports/RunEventSink.h"

// Drop-all sink for orphaned clients (architecture §2.3 / §6).
class NullEventSink final : public RunEventSink
{
public:
    void emitEvent(const RunEvent &event) override;
    void close() override;
    [[nodiscard]] bool isOpen() const override;
};

#endif // LISTENER_NULL_EVENT_SINK_H

#ifndef LISTENER_FAKES_FAKE_EVENT_SINK_H
#define LISTENER_FAKES_FAKE_EVENT_SINK_H

#include "ports/RunEventSink.h"

#include <QVector>

// Recorded sink (architecture §5).
class FakeEventSink final : public RunEventSink
{
public:
    QVector<RunEvent> events;
    int closeCount{0};
    bool open{true};

    void emitEvent(const RunEvent &event) override
    {
        if (open) {
            events.append(event);
        }
    }

    void close() override
    {
        open = false;
        ++closeCount;
    }

    [[nodiscard]] bool isOpen() const override
    {
        return open;
    }

    [[nodiscard]] QVector<RunEventType> types() const
    {
        QVector<RunEventType> out;
        out.reserve(events.size());
        for (const RunEvent &e : events) {
            out.append(e.type);
        }
        return out;
    }

    [[nodiscard]] int countOf(RunEventType type) const
    {
        int n = 0;
        for (const RunEvent &e : events) {
            if (e.type == type) {
                ++n;
            }
        }
        return n;
    }
};

#endif // LISTENER_FAKES_FAKE_EVENT_SINK_H

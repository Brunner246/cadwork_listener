#ifndef LISTENER_PORTS_RUN_EVENT_H
#define LISTENER_PORTS_RUN_EVENT_H

#include <QString>
#include <optional>

// Architecture §2.4.1 event object (common + type-specific fields).
enum class RunEventType {
    Queued,
    Started,
    Stdout,
    Stderr,
    Log,
    Heartbeat,
    Finished,
    Failed
};

struct RunEvent {
    int v{1};
    RunEventType type{RunEventType::Log};
    QString jobId;
    QString ts; // optional ISO-8601

    // type-specific (valid when relevant)
    std::optional<int> position;       // queued
    QString text;                      // stdout / stderr / log
    QString level;                     // log: info|warn|error
    std::optional<qint64> elapsedMs;   // heartbeat
    std::optional<qint64> durationMs;  // finished / failed
    std::optional<bool> ok;            // finished
    QString error;                     // failed
};

struct RunResult {
    bool ok{true};
    QString errorMessage;
};

struct RunRequest {
    QString scriptUtf8;
    // Owned sink for this job; queue may replace with NullEventSink on detach.
    // Shared so detach can re-point without invalidating callers.
    class RunEventSink *eventSink{nullptr};
};

struct RunHandle {
    QString id;
    int queuePosition{0}; // 0 = next to run (or currently starting)
};

struct OutputChunk {
    enum class Stream { Stdout, Stderr };
    Stream stream{Stream::Stdout};
    QString text;
};

struct CaptureHandles {
    QString jobId;
};

#endif // LISTENER_PORTS_RUN_EVENT_H

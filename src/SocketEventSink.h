#ifndef LISTENER_SOCKET_EVENT_SINK_H
#define LISTENER_SOCKET_EVENT_SINK_H

#include "ports/RunEventSink.h"

#include <QString>
#include <QtGlobal>

// Driven adapter (architecture §2.3): NDJSON lines on the submitting socket.
// Uses a native socket descriptor so peer write half-close (FIN) still allows replies
// (QTcpSocket on Windows treats FIN as full disconnect and rejects writes).
class SocketEventSink final : public RunEventSink
{
public:
    explicit SocketEventSink(qintptr nativeSocket);
    ~SocketEventSink() override;

    void emitEvent(const RunEvent &event) override;
    void close() override;
    [[nodiscard]] bool isOpen() const override;

    // Captured from the first event so sessions can detach by id.
    [[nodiscard]] QString jobId() const { return jobId_; }
    [[nodiscard]] qintptr nativeSocket() const { return fd_; }

private:
    qintptr fd_{-1};
    QString jobId_;
    bool open_{true};
};

#endif // LISTENER_SOCKET_EVENT_SINK_H

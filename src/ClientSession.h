//
// Created by MichaelBrunner on 22/05/2026.
//

#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <memory>

class QSocketNotifier;
class RunEventSink;
class SocketEventSink;

// Driving adapter (architecture §3.3): half-close submit + full-run socket ownership.
// Native FD path preserves write-after-FIN (QTcpSocket on Windows does not).
class ClientSession final : public QObject
{
    Q_OBJECT

public:
    // Takes ownership of nativeSocket (closes it via SocketEventSink or dtor).
    explicit ClientSession(qintptr nativeSocket, QObject *parent = nullptr);
    ~ClientSession() override;

    [[nodiscard]] QString jobId() const;
    [[nodiscard]] bool hasSubmitted() const { return submitted_; }

signals:
    void runSubmitted(const QByteArray &script, RunEventSink *sink);
    void clientDetached(const QString &jobId);

private slots:
    void onNativeReadable();

private:
    void submitIfNeeded();
    void notifyDetachedAndFinish();
    void finishAfterTrailer();

    qintptr fd_{-1};
    QSocketNotifier *readNotifier_{nullptr};
    QByteArray buffer_;
    std::unique_ptr<SocketEventSink> sink_;
    bool submitted_{false};
    bool finished_{false};
};

#endif // CLIENTSESSION_H

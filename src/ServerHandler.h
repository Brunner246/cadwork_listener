//
// Created by MichaelBrunner on 19/02/2025.
//

#ifndef SERVERHANDLER_H
#define SERVERHANDLER_H

#include <QHostAddress>
#include <QObject>
#include <QtGlobal>

namespace CwAPI3D::Interfaces
{
class ICwAPI3DUtilityController;
}

class QSocketNotifier;
class ScriptExecutor;
class ScriptQueue;
class NoopOutputCapture;

// Composition root (architecture §2.3 / §6): ScriptQueue + adapters, LocalHost:9999.
// Native listen/accept so client half-close remains writable for NDJSON replies.
class ServerHandler final : public QObject
{
    Q_OBJECT

public:
    static constexpr quint16 kPort = 9999;

    explicit ServerHandler(CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController,
                           QObject *parent = nullptr);

    ~ServerHandler() override;

    void runEventLoop() const;

    [[nodiscard]] bool isListening() const { return listening_; }
    [[nodiscard]] QHostAddress serverAddress() const { return QHostAddress::LocalHost; }
    [[nodiscard]] quint16 serverPort() const { return listening_ ? kPort : 0; }

signals:
    void serverStopped();

private slots:
    void onAcceptable();
    void onRunSubmitted(const QByteArray &script, class RunEventSink *sink);
    void onClientDetached(const QString &jobId);

private:
    bool startListening();
    void stopListening();

    qintptr listenFd_{-1};
    QSocketNotifier *acceptNotifier_{nullptr};
    bool listening_{false};

    ScriptExecutor *executor{nullptr};
    NoopOutputCapture *capture_{nullptr};
    ScriptQueue *queue_{nullptr};
};

#endif // SERVERHANDLER_H

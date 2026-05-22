//
// Created by MichaelBrunner on 22/05/2026.
//

#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <QObject>
#include <QByteArray>

class QTcpSocket;

class ClientSession final : public QObject
{
    Q_OBJECT

public:
    explicit ClientSession(QTcpSocket *socket, QObject *parent = nullptr);
    ~ClientSession() override = default;

signals:
    void scriptReceived(const QByteArray &script);

private slots:
    void onReadyRead();
    void onDisconnected();
    void onErrorOccurred() const;

private:
    QTcpSocket *socket{nullptr};
    QByteArray buffer;
    bool closed{false};
};

#endif //CLIENTSESSION_H

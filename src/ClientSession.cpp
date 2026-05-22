//
// Created by MichaelBrunner on 22/05/2026.
//

#include "ClientSession.h"

#include <QTcpSocket>
#include <QDebug>

ClientSession::ClientSession(QTcpSocket *socket, QObject *parent)
    : QObject(parent),
      socket(socket)
{
    socket->setParent(this);
    connect(socket, &QTcpSocket::readyRead, this, &ClientSession::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &ClientSession::onDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &ClientSession::onErrorOccurred);
}

void ClientSession::onReadyRead()
{
    buffer.append(socket->readAll());
}

void ClientSession::onDisconnected()
{
    if (closed) {
        return;
    }
    closed = true;
    buffer.append(socket->readAll());
    if (!buffer.isEmpty()) {
        emit scriptReceived(buffer);
    }
    qInfo() << "Client Disconnected";
    deleteLater();
}

void ClientSession::onErrorOccurred() const
{
    if (socket->error() != QAbstractSocket::RemoteHostClosedError) {
        qWarning() << "ClientSession error:" << socket->errorString();
    }
}

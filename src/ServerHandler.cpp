//
// Created by MichaelBrunner on 19/02/2025.
//

#include "ServerHandler.h"
#include "ClientSession.h"
#include "ScriptExecutor.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QEventLoop>
#include <QDebug>

ServerHandler::ServerHandler(CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController, QObject *parent)
    : QObject(parent),
      server(new QTcpServer(this)),
      executor(new ScriptExecutor(utilityController, this))
{
    connect(server, &QTcpServer::newConnection, this, &ServerHandler::handleNewConnection);

    if (!server->listen(QHostAddress::Any, 9999)) {
        qCritical() << "Server failed to start: " << server->errorString();
    }
    else {
        qInfo() << "Server listening on port 9999...";
    }
}

void ServerHandler::runEventLoop() const
{
    QEventLoop loop;
    connect(this, &ServerHandler::serverStopped, &loop, &QEventLoop::quit);
    loop.exec();
}

void ServerHandler::handleNewConnection()
{
    QTcpSocket *socket = server->nextPendingConnection();
    if (!socket) {
        return;
    }
    qInfo() << "Client Connected!";

    const auto *session = new ClientSession(socket, this);
    connect(session,
            &ClientSession::scriptReceived,
            executor,
            &ScriptExecutor::executeScript,
            Qt::DirectConnection);
}

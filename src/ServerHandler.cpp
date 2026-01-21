//
// Created by MichaelBrunner on 19/02/2025.
//

#include "ServerHandler.h"

ServerHandler::ServerHandler(CwAPI3D::UtilityController *utilityController, QObject *parent)
    : QObject(parent),
      server(new QTcpServer(this)),
      utilityController(utilityController)
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
    QTcpSocket *clientSocket = server->nextPendingConnection();
    qInfo() << "Client Connected!";

    connect(clientSocket,
            &QTcpSocket::disconnected,
            this,
            [=]
            {
                qInfo() << "Client Disconnected";
                clientSocket->deleteLater();
            });

    connect(clientSocket,
            &QTcpSocket::readyRead,
            this,
            [this, clientSocket]
            {
                const QByteArray data = clientSocket->readAll();
                const QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                    "/cw_script.py";
                QFile file(tempPath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    file.write(data);
                    file.close();
                }
                if (file.exists()) {
                    utilityController->runExternalProgramFromCustomDirectory(tempPath.toStdWString().c_str());
                }
                QFile::remove(tempPath);
            });
}

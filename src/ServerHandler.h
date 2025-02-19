//
// Created by MichaelBrunner on 19/02/2025.
//

#ifndef SERVERHANDLER_H
#define SERVERHANDLER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QVBoxLayout>
#include <QLabel>
#include <QFile>
#include <QStandardPaths>
#include <QEventLoop>
#include <cwapi3d/CwAPI3D.h>

class ServerHandler final: public QObject
{
    Q_OBJECT

public:
    explicit ServerHandler(CwAPI3D::UtilityController *utilityController, QObject *parent = nullptr);

    ~ServerHandler() override = default;

    void runEventLoop() const;

signals:
    void serverStopped();

private slots:
    void handleNewConnection();

private:
    QTcpServer *server{nullptr};
    CwAPI3D::UtilityController *utilityController{nullptr};
};

#endif //SERVERHANDLER_H

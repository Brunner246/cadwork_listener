//
// Created by MichaelBrunner on 19/02/2025.
//

#ifndef SERVERHANDLER_H
#define SERVERHANDLER_H

#include <QObject>

namespace CwAPI3D::Interfaces
{
class ICwAPI3DUtilityController;
}

class QTcpServer;
class ScriptExecutor;

class ServerHandler final : public QObject
{
    Q_OBJECT

public:
    explicit ServerHandler(CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController, QObject *parent = nullptr);

    ~ServerHandler() override = default;

    void runEventLoop() const;

signals:
    void serverStopped();

private slots:
    void handleNewConnection();

private:
    QTcpServer *server{nullptr};
    ScriptExecutor *executor{nullptr};
};

#endif //SERVERHANDLER_H

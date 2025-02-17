#include <QDialog>
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
    explicit ServerHandler(CwAPI3D::UtilityController *utilityController, QObject *parent = nullptr)
        : QObject(parent),
          server(new QTcpServer(this)),
          utilityController(utilityController)
    {
        connect(server, &QTcpServer::newConnection, this, &ServerHandler::handleNewConnection);

        if (!server->listen(QHostAddress::Any, 9999)) {
            qDebug() << "Server failed to start: " << server->errorString();
        } else {
            qDebug() << "Server listening on port 9999...";
        }
    }

    void runEventLoop() const
    {
        QEventLoop loop;
        connect(this, &ServerHandler::serverStopped, &loop, &QEventLoop::quit);
        loop.exec();
    }

signals:
    void serverStopped();

private slots:
    void handleNewConnection()
    {
        QTcpSocket *clientSocket = server->nextPendingConnection();
        qDebug() << "Client Connected!";

        connect(clientSocket, &QTcpSocket::disconnected, this, [=]() {
            qDebug() << "Client Disconnected";
            clientSocket->deleteLater();
        });

        connect(clientSocket, &QTcpSocket::readyRead, this, [=]() {
            const QByteArray data = clientSocket->readAll();
            qDebug() << "Data Received: " << data;

            const QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/cw_script.py";
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

private:
    QTcpServer *server{nullptr};
    CwAPI3D::UtilityController *utilityController{nullptr};
};

CWAPI3D_PLUGIN bool plugin_x64_init(CwAPI3D::ControllerFactory *aFactory)
{
    const auto serverHandler = new ServerHandler(aFactory->getUtilityController());
    serverHandler->runEventLoop();

    return false;
}

#include "library.moc"

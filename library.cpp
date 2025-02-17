#include <QDialog>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVBoxLayout>
#include <QLabel>
#include <QFile>
#include <QStandardPaths>

#include <cwapi3d/CwAPI3D.h>

class ServerDialog final: public QDialog
{
    Q_OBJECT

public:
    explicit ServerDialog(CwAPI3D::UtilityController *utilityController, QWidget *parent = nullptr)
        : QDialog(parent),
          server(new QTcpServer(this)),
          utilityController(utilityController)
    {
        setWindowTitle("TCP Server");
        resize(300, 150);

        const auto layout = new QVBoxLayout(this);
        statusLabel = new QLabel("Waiting for connection...", this);
        layout->addWidget(statusLabel);

        connect(server, &QTcpServer::newConnection, this, &ServerDialog::handleNewConnection);
        if (!server->listen(QHostAddress::Any, 9999)) {
            qDebug() << "Server failed to start: " << server->errorString();
        }

        setAttribute(Qt::WA_DeleteOnClose);
    }

private slots:
    void handleNewConnection()
    {
        QTcpSocket *clientSocket = server->nextPendingConnection();
        statusLabel->setText("Client Connected!");

        connect(clientSocket,
                &QTcpSocket::disconnected,
                this,
                [=]()
                {
                    statusLabel->setText("Client Disconnected");
                    clientSocket->deleteLater();
                });

        connect(clientSocket,
                &QTcpSocket::readyRead,
                this,
                [=]()
                {
                    const QByteArray data = clientSocket->readAll();
                    statusLabel->setText("Data Received");

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

private:
    QTcpServer *server{nullptr};
    QLabel *statusLabel{nullptr};
    CwAPI3D::UtilityController *utilityController{nullptr};
};

CWAPI3D_PLUGIN bool plugin_x64_init(CwAPI3D::ControllerFactory *aFactory)
{
    const auto serverDialog = new ServerDialog(aFactory->getUtilityController());
    serverDialog->show();

    return false;
}

#include "library.moc"

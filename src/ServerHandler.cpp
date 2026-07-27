//
// Created by MichaelBrunner on 19/02/2025.
//

#include "ServerHandler.h"
#include "ClientSession.h"
#include "NoopOutputCapture.h"
#include "ScriptExecutor.h"
#include "ScriptQueue.h"
#include "ports/RunEvent.h"

#include <QDebug>
#include <QEventLoop>
#include <QSocketNotifier>

#if defined(Q_OS_WIN)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace {

#if defined(Q_OS_WIN)
class WinsockLifetime
{
public:
    WinsockLifetime()
    {
        WSADATA wsa{};
        ok_ = (::WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    }
    ~WinsockLifetime()
    {
        if (ok_) {
            ::WSACleanup();
        }
    }
    bool ok() const { return ok_; }

private:
    bool ok_{false};
};

WinsockLifetime &winsockLifetime()
{
    static WinsockLifetime lifetime;
    return lifetime;
}
#endif

void setNonBlocking(qintptr fd)
{
#if defined(Q_OS_WIN)
    u_long mode = 1;
    ::ioctlsocket(SOCKET(fd), FIONBIO, &mode);
#else
    const int flags = ::fcntl(int(fd), F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(int(fd), F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

} // namespace

ServerHandler::ServerHandler(CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController,
                             QObject *parent)
    : QObject(parent),
      executor(new ScriptExecutor(utilityController, this)),
      capture_(new NoopOutputCapture()),
      queue_(new ScriptQueue(executor, capture_, this))
{
#if defined(Q_OS_WIN)
    if (!winsockLifetime().ok()) {
        qCritical() << "ServerHandler: WSAStartup failed";
        return;
    }
#endif
    if (!startListening()) {
        qCritical() << "Server failed to start on LocalHost:" << kPort;
    } else {
        qInfo() << "Server listening on 127.0.0.1 port" << kPort;
    }
}

ServerHandler::~ServerHandler()
{
    stopListening();
    delete capture_;
    capture_ = nullptr;
}

bool ServerHandler::startListening()
{
#if defined(Q_OS_WIN)
    const SOCKET fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET) {
        return false;
    }
    BOOL reuse = TRUE;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#else
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }
    int reuse = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // US-18 / architecture §6

#if defined(Q_OS_WIN)
    if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        ::closesocket(fd);
        return false;
    }
    if (::listen(fd, SOMAXCONN) == SOCKET_ERROR) {
        ::closesocket(fd);
        return false;
    }
    listenFd_ = qintptr(fd);
#else
    if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return false;
    }
    if (::listen(fd, SOMAXCONN) != 0) {
        ::close(fd);
        return false;
    }
    listenFd_ = qintptr(fd);
#endif

    setNonBlocking(listenFd_);
    acceptNotifier_ = new QSocketNotifier(listenFd_, QSocketNotifier::Read, this);
    connect(acceptNotifier_, &QSocketNotifier::activated, this, &ServerHandler::onAcceptable);
    listening_ = true;
    return true;
}

void ServerHandler::stopListening()
{
    if (acceptNotifier_) {
        acceptNotifier_->setEnabled(false);
        acceptNotifier_->deleteLater();
        acceptNotifier_ = nullptr;
    }
    if (listenFd_ >= 0) {
#if defined(Q_OS_WIN)
        ::closesocket(SOCKET(listenFd_));
#else
        ::close(int(listenFd_));
#endif
        listenFd_ = -1;
    }
    listening_ = false;
}

void ServerHandler::runEventLoop() const
{
    QEventLoop loop;
    connect(this, &ServerHandler::serverStopped, &loop, &QEventLoop::quit);
    loop.exec();
}

void ServerHandler::onAcceptable()
{
    if (listenFd_ < 0) {
        return;
    }

    for (;;) {
#if defined(Q_OS_WIN)
        const SOCKET client = ::accept(SOCKET(listenFd_), nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            const int err = ::WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                qWarning() << "ServerHandler: accept failed" << err;
            }
            return;
        }
        const qintptr clientFd = qintptr(client);
#else
        const int client = ::accept(int(listenFd_), nullptr, nullptr);
        if (client < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                qWarning() << "ServerHandler: accept failed" << errno;
            }
            return;
        }
        const qintptr clientFd = qintptr(client);
#endif

        qInfo() << "Client Connected!";
        auto *session = new ClientSession(clientFd, this);
        connect(session, &ClientSession::runSubmitted, this, &ServerHandler::onRunSubmitted);
        connect(session, &ClientSession::clientDetached, this, &ServerHandler::onClientDetached);
    }
}

void ServerHandler::onRunSubmitted(const QByteArray &script, RunEventSink *sink)
{
    // SubmitRun via queue only — no DirectConnection to ScriptExecutor (research §2).
    RunRequest request;
    request.scriptUtf8 = QString::fromUtf8(script);
    request.eventSink = sink;
    queue_->enqueue(request);
}

void ServerHandler::onClientDetached(const QString &jobId)
{
    queue_->onClientDetached(jobId);
}

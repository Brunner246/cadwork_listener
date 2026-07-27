//
// Created by MichaelBrunner on 22/05/2026.
//

#include "ClientSession.h"
#include "SocketEventSink.h"

#include <QDebug>
#include <QSocketNotifier>

#if defined(Q_OS_WIN)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#else
#  include <errno.h>
#  include <fcntl.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace {

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

ClientSession::ClientSession(qintptr nativeSocket, QObject *parent)
    : QObject(parent),
      fd_(nativeSocket)
{
    if (fd_ < 0) {
        qWarning() << "ClientSession: invalid socket";
        finished_ = true;
        deleteLater();
        return;
    }
    setNonBlocking(fd_);

    readNotifier_ = new QSocketNotifier(fd_, QSocketNotifier::Read, this);
    connect(readNotifier_, &QSocketNotifier::activated, this, &ClientSession::onNativeReadable);

    // Defer first read so callers (ServerHandler / tests) can connect runSubmitted
    // before we process a body that is already fully buffered + FIN'd.
    QMetaObject::invokeMethod(this, &ClientSession::onNativeReadable, Qt::QueuedConnection);
}

ClientSession::~ClientSession()
{
    if (readNotifier_) {
        readNotifier_->setEnabled(false);
    }
    if (!sink_ && fd_ >= 0) {
#if defined(Q_OS_WIN)
        ::closesocket(SOCKET(fd_));
#else
        ::close(int(fd_));
#endif
        fd_ = -1;
    }
}

QString ClientSession::jobId() const
{
    return sink_ ? sink_->jobId() : QString();
}

void ClientSession::onNativeReadable()
{
    if (finished_ || fd_ < 0) {
        return;
    }

    char buf[16384];
    for (;;) {
#if defined(Q_OS_WIN)
        const int n = ::recv(SOCKET(fd_), buf, sizeof(buf), 0);
#else
        const int n = static_cast<int>(::recv(int(fd_), buf, sizeof(buf), 0));
#endif
        if (n > 0) {
            if (!submitted_) {
                buffer_.append(buf, n);
            }
            continue;
        }
        if (n == 0) {
            if (!submitted_) {
                submitIfNeeded();
            } else if (!finished_) {
                notifyDetachedAndFinish();
            }
            return;
        }

#if defined(Q_OS_WIN)
        const int err = ::WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
#endif
        if (!submitted_) {
            submitIfNeeded();
            if (finished_) {
                return;
            }
        }
        if (!finished_) {
            notifyDetachedAndFinish();
        }
        return;
    }
}

void ClientSession::submitIfNeeded()
{
    if (submitted_ || finished_) {
        return;
    }
    submitted_ = true;

    // Closing the peer FD (after trailer) can re-enter onNativeReadable via QSocketNotifier
    // while we are still inside enqueue → treat that as detach and drop the reply path.
    if (readNotifier_) {
        readNotifier_->setEnabled(false);
    }

    sink_ = std::make_unique<SocketEventSink>(fd_);
    emit runSubmitted(buffer_, sink_.get());

    if (finished_) {
        return;
    }

    if (!sink_->isOpen()) {
        finishAfterTrailer();
        return;
    }

    // Job still running (e.g. held FakeScriptRunner): watch for mid-run peer close.
    if (readNotifier_) {
        readNotifier_->setEnabled(true);
    }
}

void ClientSession::finishAfterTrailer()
{
    finished_ = true;
    if (readNotifier_) {
        readNotifier_->setEnabled(false);
    }
    fd_ = -1;
    qInfo() << "Client session complete after trailer";
    deleteLater();
}

void ClientSession::notifyDetachedAndFinish()
{
    if (finished_) {
        return;
    }
    finished_ = true;
    if (readNotifier_) {
        readNotifier_->setEnabled(false);
    }

    const QString id = jobId();
    if (submitted_ && !id.isEmpty()) {
        emit clientDetached(id);
    }

    if (sink_ && sink_->isOpen()) {
        sink_->close();
    }
    fd_ = -1;

    qInfo() << "Client Disconnected";
    deleteLater();
}

#include "SocketEventSink.h"
#include "ProtocolCodec.h"

#if defined(Q_OS_WIN)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#  include <unistd.h>
#endif

SocketEventSink::SocketEventSink(const qintptr nativeSocket)
    : fd_(nativeSocket)
{
}

SocketEventSink::~SocketEventSink()
{
    close();
}

void SocketEventSink::emitEvent(const RunEvent &event)
{
    if (jobId_.isEmpty() && !event.jobId.isEmpty()) {
        jobId_ = event.jobId;
    }
    if (!isOpen()) {
        return;
    }
    const QByteArray line = ProtocolCodec::encode(event);
    const char *p = line.constData();
    int left = static_cast<int>(line.size());
    while (left > 0) {
#if defined(Q_OS_WIN)
        const int n = ::send(static_cast<SOCKET>(fd_), p, left, 0);
#else
        const int n = static_cast<int>(::send(int(fd_), p, size_t(left), 0));
#endif
        if (n <= 0) {
            open_ = false;
            return;
        }
        p += n;
        left -= n;
    }
}

void SocketEventSink::close()
{
    if (!open_ && fd_ < 0) {
        return;
    }
    open_ = false;
    if (fd_ >= 0) {
        // Graceful write-side FIN so NDJSON already in the send buffer is not RST'd away
        // (closesocket alone can abort small, fast trailer payloads on Windows).
#if defined(Q_OS_WIN)
        ::shutdown(static_cast<SOCKET>(fd_), SD_SEND);
        LINGER lingerOpts{};
        lingerOpts.l_onoff = 1;
        lingerOpts.l_linger = 2; // seconds
        ::setsockopt(static_cast<SOCKET>(fd_), SOL_SOCKET, SO_LINGER, reinterpret_cast<const char *>(&lingerOpts),
                     sizeof(lingerOpts));
        ::closesocket(static_cast<SOCKET>(fd_));
#else
        ::shutdown(int(fd_), SHUT_WR);
        ::close(int(fd_));
#endif
        fd_ = -1;
    }
}

bool SocketEventSink::isOpen() const
{
    return open_ && fd_ >= 0;
}

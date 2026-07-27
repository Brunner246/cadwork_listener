#include "ClientSession.h"
#include "ProtocolCodec.h"
#include "ScriptQueue.h"
#include "ServerHandler.h"

#include "fakes/FakeOutputCapture.h"
#include "fakes/FakeScriptRunner.h"

#include <QtTest>

#include <QElapsedTimer>
#include <QFile>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>

#if defined(Q_OS_WIN)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
using SOCKET = int;
static constexpr int INVALID_SOCKET = -1;
static constexpr int SOCKET_ERROR = -1;
static int closesocket(int s) { return ::close(s); }
#endif

namespace {

// Process-wide Winsock — never pair Cleanup per object (avoids tearing down other sockets).
void ensureWinsock()
{
#if defined(Q_OS_WIN)
    static const bool ok = []() {
        WSADATA wsa{};
        return ::WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }();
    Q_UNUSED(ok);
#endif
}

void setNonBlocking(SOCKET fd)
{
#if defined(Q_OS_WIN)
    u_long mode = 1;
    ::ioctlsocket(fd, FIONBIO, &mode);
#else
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

class NativeLoopbackServer
{
public:
    NativeLoopbackServer() { ensureWinsock(); }

    ~NativeLoopbackServer() { stop(); }

    bool listen()
    {
#if defined(Q_OS_WIN)
        listenFd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
        listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
#endif
        if (listenFd_ == INVALID_SOCKET) {
            return false;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listenFd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            return false;
        }
        if (::listen(listenFd_, 16) == SOCKET_ERROR) {
            return false;
        }
        setNonBlocking(listenFd_);
        int alen = sizeof(addr);
        ::getsockname(listenFd_, reinterpret_cast<sockaddr *>(&addr),
#if defined(Q_OS_WIN)
                      &alen
#else
                      reinterpret_cast<socklen_t *>(&alen)
#endif
        );
        port_ = ntohs(addr.sin_port);
        return true;
    }

    [[nodiscard]] quint16 port() const { return port_; }

    // Non-blocking accept; returns -1 when none pending.
    qintptr tryAccept() const
    {
        if (listenFd_ == INVALID_SOCKET) {
            return -1;
        }
#if defined(Q_OS_WIN)
        const SOCKET client = ::accept(listenFd_, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            return -1;
        }
        return qintptr(client);
#else
        const int client = ::accept(listenFd_, nullptr, nullptr);
        if (client < 0) {
            return -1;
        }
        return qintptr(client);
#endif
    }

    void stop()
    {
        if (listenFd_ != INVALID_SOCKET) {
            closesocket(listenFd_);
            listenFd_ = INVALID_SOCKET;
        }
    }

private:
    SOCKET listenFd_{INVALID_SOCKET};
    quint16 port_{0};
};

class RawTcpClient
{
public:
    RawTcpClient() { ensureWinsock(); }

    ~RawTcpClient() { close(); }

    bool connectTo(const quint16 port)
    {
        sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock_ == INVALID_SOCKET) {
            return false;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        return ::connect(sock_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != SOCKET_ERROR;
    }

    bool writeAll(const QByteArray &data) const
    {
        const char *p = data.constData();
        int left = data.size();
        while (left > 0) {
            const int n = ::send(sock_, p, left, 0);
            if (n == SOCKET_ERROR || n == 0) {
                return false;
            }
            p += n;
            left -= n;
        }
        return true;
    }

    bool halfCloseWrite()
    {
#if defined(Q_OS_WIN)
        return ::shutdown(sock_, SD_SEND) != SOCKET_ERROR;
#else
        return ::shutdown(sock_, SHUT_WR) != SOCKET_ERROR;
#endif
    }

    bool fullClose()
    {
        if (sock_ == INVALID_SOCKET) {
            return true;
        }
        const int rc = closesocket(sock_);
        sock_ = INVALID_SOCKET;
        return rc != SOCKET_ERROR;
    }

    QByteArray readSome(const int timeoutMs) const
    {
        if (sock_ == INVALID_SOCKET) {
            return {};
        }
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock_, &fds);
        timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
#if defined(Q_OS_WIN)
        const int sel = ::select(0, &fds, nullptr, nullptr, &tv);
#else
        const int sel = ::select(sock_ + 1, &fds, nullptr, nullptr, &tv);
#endif
        if (sel <= 0) {
            return {};
        }
        char buf[8192];
        const int n = ::recv(sock_, buf, sizeof(buf), 0);
        if (n <= 0) {
            return {};
        }
        return QByteArray(buf, n);
    }

    void close()
    {
        if (sock_ != INVALID_SOCKET) {
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
        }
    }

private:
    SOCKET sock_{INVALID_SOCKET};
};

QByteArray loadFixture(const char *relativePath)
{
    QFile f(QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + QLatin1String(relativePath));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

struct NdjsonStream {
    QByteArray raw;
    QVector<RunEvent> events;

    bool feed(const QByteArray &chunk)
    {
        raw.append(chunk);
        while (true) {
            const int nl = raw.indexOf('\n');
            if (nl < 0) {
                break;
            }
            const QByteArray line = raw.left(nl);
            raw.remove(0, nl + 1);
            if (line.trimmed().isEmpty()) {
                continue;
            }
            const auto ev = ProtocolCodec::decodeLine(line);
            if (!ev.has_value()) {
                return false;
            }
            events.append(*ev);
        }
        return true;
    }

    [[nodiscard]] int trailerCount() const
    {
        int n = 0;
        for (const RunEvent &e : events) {
            if (ProtocolCodec::isTrailer(e)) {
                ++n;
            }
        }
        return n;
    }

    [[nodiscard]] std::optional<RunEvent> trailer() const
    {
        for (const RunEvent &e : events) {
            if (ProtocolCodec::isTrailer(e)) {
                return e;
            }
        }
        return std::nullopt;
    }
};

struct SessionHarness {
    FakeScriptRunner runner;
    FakeOutputCapture capture;
    ScriptQueue queue{&runner, &capture};
    NativeLoopbackServer server;
    QObject context;
    QByteArray lastBody;
    int acceptCount{0};

    bool start()
    {
        if (!server.listen()) {
            return false;
        }
        return true;
    }

    // Accept pending clients and wire them to the queue.
    void pumpAccepts()
    {
        for (;;) {
            const qintptr fd = server.tryAccept();
            if (fd < 0) {
                break;
            }
            ++acceptCount;
            auto *session = new ClientSession(fd, &context);
            QObject::connect(session, &ClientSession::runSubmitted, &context,
                             [this](const QByteArray &script, RunEventSink *sink) {
                                 lastBody = script;
                                 queue.enqueue(RunRequest{QString::fromUtf8(script), sink});
                             });
            QObject::connect(session, &ClientSession::clientDetached, &queue,
                             &ScriptQueue::onClientDetached);
            // Session may already have readable data/FIN; process Qt notifiers.
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
    }

    bool waitForTrailer(RawTcpClient &client, NdjsonStream &stream, const int timeoutMs = 5000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            pumpAccepts();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            const QByteArray chunk = client.readSome(20);
            if (!chunk.isEmpty()) {
                if (!stream.feed(chunk)) {
                    return false;
                }
            }
            if (stream.trailerCount() >= 1) {
                return true;
            }
        }
        return stream.trailerCount() >= 1;
    }
};

} // namespace

class SessionServerTest : public QObject
{
    Q_OBJECT

private slots:
    void listenPortIsLoopbackConstant();
    void halfCloseMultilineBodyIntegrity();
    void halfCloseLargeBodyIntegrity();
    void emptyBodyFailsWithoutRunner();
    void streamEndsWithSingleTrailer();
    void disconnectMidRunSecondClientCompletes();
    void fullCloseStillRunsViaOrphanPath();
};

void SessionServerTest::listenPortIsLoopbackConstant()
{
    QCOMPARE(ServerHandler::kPort, quint16(9999));
    NativeLoopbackServer probe;
    QVERIFY(probe.listen());
    QVERIFY(probe.port() != 0);
}

void SessionServerTest::halfCloseMultilineBodyIntegrity()
{
    const QByteArray body = loadFixture("scripts/multiline_utf8.py.txt");
    QVERIFY(!body.isEmpty());

    SessionHarness h;
    QVERIFY(h.start());

    RawTcpClient client;
    QVERIFY(client.connectTo(h.server.port()));
    QVERIFY(client.writeAll(body));
    QVERIFY(client.halfCloseWrite());

    NdjsonStream stream;
    QVERIFY(h.waitForTrailer(client, stream));
    QCOMPARE(h.acceptCount, 1);
    QCOMPARE(h.lastBody, body);
    QCOMPARE(stream.trailerCount(), 1);
    QCOMPARE(stream.trailer()->type, RunEventType::Finished);
    QCOMPARE(h.runner.calls.size(), 1);
    QCOMPARE(h.runner.calls.first().scriptUtf8.toUtf8(), body);
}

void SessionServerTest::halfCloseLargeBodyIntegrity()
{
    const QByteArray body = loadFixture("scripts/large_body.py.txt");
    QVERIFY(body.size() > 10'000);

    SessionHarness h;
    QVERIFY(h.start());

    RawTcpClient client;
    QVERIFY(client.connectTo(h.server.port()));
    QVERIFY(client.writeAll(body));
    QVERIFY(client.halfCloseWrite());

    NdjsonStream stream;
    QVERIFY(h.waitForTrailer(client, stream));
    QCOMPARE(h.lastBody.size(), body.size());
    QCOMPARE(h.lastBody, body);
    QCOMPARE(stream.trailerCount(), 1);
    QCOMPARE(h.runner.calls.size(), 1);
    QCOMPARE(h.runner.calls.first().scriptUtf8.toUtf8(), body);
}

void SessionServerTest::emptyBodyFailsWithoutRunner()
{
    SessionHarness h;
    QVERIFY(h.start());

    RawTcpClient client;
    QVERIFY(client.connectTo(h.server.port()));
    QVERIFY(client.halfCloseWrite());

    NdjsonStream stream;
    QVERIFY(h.waitForTrailer(client, stream));
    QCOMPARE(stream.trailerCount(), 1);
    QCOMPARE(stream.trailer()->type, RunEventType::Failed);
    QCOMPARE(h.runner.calls.size(), 0);
    QVERIFY(h.queue.activeJobId().isEmpty());
}

void SessionServerTest::streamEndsWithSingleTrailer()
{
    SessionHarness h;
    QVERIFY(h.start());

    RawTcpClient client;
    QVERIFY(client.connectTo(h.server.port()));
    const QByteArray body = QByteArrayLiteral("print('ok')\n");
    QVERIFY(client.writeAll(body));
    QVERIFY(client.halfCloseWrite());

    NdjsonStream stream;
    QVERIFY(h.waitForTrailer(client, stream));
    QCOMPARE(stream.trailerCount(), 1);
    QVERIFY(stream.trailer()->type == RunEventType::Finished
            || stream.trailer()->type == RunEventType::Failed);

    QElapsedTimer extra;
    extra.start();
    while (extra.elapsed() < 150) {
        h.pumpAccepts();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        const QByteArray chunk = client.readSome(20);
        if (!chunk.isEmpty()) {
            QVERIFY(stream.feed(chunk));
        }
    }
    QCOMPARE(stream.trailerCount(), 1);
}

void SessionServerTest::disconnectMidRunSecondClientCompletes()
{
    SessionHarness h;
    h.runner.holdRun = true;
    QVERIFY(h.start());

    RawTcpClient clientA;
    QVERIFY(clientA.connectTo(h.server.port()));

    // Work posted BEFORE entering the hold loop (same pattern as script_queue_test).
    // release() only quits the nested loop; A/B completion is observed after the hold returns.
    auto clientB = std::make_shared<RawTcpClient>();
    bool released = false;
    QString midRunError;
    const auto midRun = std::shared_ptr<std::function<void()>>(new std::function<void()>);
    *midRun = [&]() {
        if (h.runner.currentConcurrent < 1) {
            QTimer::singleShot(5, &h.context, [midRun]() { (*midRun)(); });
            return;
        }

        // Orphan A while host run still held — US-11.
        h.queue.onClientDetached(h.runner.calls[0].jobId);
        clientA.fullClose();

        if (!clientB->connectTo(h.server.port())) {
            midRunError = QStringLiteral("clientB connect failed");
            h.runner.holdRun = false;
            h.runner.release();
            released = true;
            return;
        }
        const QByteArray bodyB = QByteArrayLiteral("print('B')\n");
        if (!clientB->writeAll(bodyB) || !clientB->halfCloseWrite()) {
            midRunError = QStringLiteral("clientB write/half-close failed");
            h.runner.holdRun = false;
            h.runner.release();
            released = true;
            return;
        }

        QElapsedTimer pendingWait;
        pendingWait.start();
        while (pendingWait.elapsed() < 2000 && h.queue.pendingCount() < 1) {
            h.pumpAccepts();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        if (h.queue.pendingCount() < 1) {
            midRunError = QStringLiteral("B never queued");
        }

        h.runner.holdRun = false;
        h.runner.release(); // quit hold; A finishes + B runs after stack unwinds
        released = true;
    };
    QTimer::singleShot(0, &h.context, [midRun]() { (*midRun)(); });

    const QByteArray bodyA = QByteArrayLiteral("print('A')\n");
    QVERIFY(clientA.writeAll(bodyA));
    QVERIFY(clientA.halfCloseWrite());

    // Enter hold via processEvents; midRun releases; then both jobs complete.
    QElapsedTimer outer;
    outer.start();
    while (outer.elapsed() < 10000 && h.runner.calls.size() < 2) {
        h.pumpAccepts();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    QVERIFY2(midRunError.isEmpty(), qPrintable(midRunError));
    QVERIFY2(released, "mid-run release never ran");
    QCOMPARE(h.runner.calls.size(), 2);
    QCOMPARE(h.runner.calls[0].scriptUtf8, QStringLiteral("print('A')\n"));
    QCOMPARE(h.runner.calls[1].scriptUtf8, QStringLiteral("print('B')\n"));
    QVERIFY(h.queue.activeJobId().isEmpty());
    QCOMPARE(h.queue.pendingCount(), 0);

    NdjsonStream streamB;
    QVERIFY(h.waitForTrailer(*clientB, streamB, 3000));
    QCOMPARE(streamB.trailerCount(), 1);
    QCOMPARE(streamB.trailer()->type, RunEventType::Finished);
}

void SessionServerTest::fullCloseStillRunsViaOrphanPath()
{
    SessionHarness h;
    QVERIFY(h.start());

    RawTcpClient client;
    QVERIFY(client.connectTo(h.server.port()));
    const QByteArray body = QByteArrayLiteral("print('legacy')\n");
    QVERIFY(client.writeAll(body));
    QVERIFY(client.fullClose());

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000 && h.runner.calls.isEmpty()) {
        h.pumpAccepts();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    QCOMPARE(h.runner.calls.size(), 1);
    QCOMPARE(h.runner.calls.first().scriptUtf8.toUtf8(), body);
    QVERIFY(h.queue.activeJobId().isEmpty());
}

QObject *createSessionServerTest()
{
    return new SessionServerTest;
}

#include "session_server_test.moc"

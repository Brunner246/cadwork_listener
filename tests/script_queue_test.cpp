#include "ScriptQueue.h"
#include "ProtocolCodec.h"

#include "fakes/FakeEventSink.h"
#include "fakes/FakeOutputCapture.h"
#include "fakes/FakeScriptRunner.h"

#include <QtTest>

#include <QTimer>

class ScriptQueueTest : public QObject
{
    Q_OBJECT

private slots:
    void singleActiveNeverOverlaps();
    void fifoSecondStartsAfterFirstTrailer();
    void enqueueWhileRunningPosition();
    void detachMidRunThenNextCompletes();
    void detachWhileQueuedDoesNotStartHost();
    void failedJobThenNextCompletes();
    void heartbeatUnderQuietHeldRun();
    void captureChunksBecomeStdoutStderr();
    void emptyScriptFailsWithoutRunner();
};

void ScriptQueueTest::singleActiveNeverOverlaps()
{
    FakeScriptRunner runner;
    runner.holdRun = true;
    FakeOutputCapture capture;
    FakeEventSink sinkA;
    FakeEventSink sinkB;
    ScriptQueue queue(&runner, &capture);

    QTimer::singleShot(0, [&]() {
        queue.enqueue(RunRequest{QStringLiteral("print(2)"), &sinkB});
        QCOMPARE(runner.currentConcurrent, 1);
        runner.holdRun = false; // B must complete without a second hold
        runner.release();
    });

    queue.enqueue(RunRequest{QStringLiteral("print(1)"), &sinkA});

    QCOMPARE(runner.maxConcurrent, 1);
    QCOMPARE(runner.calls.size(), 2);
    QVERIFY(sinkA.countOf(RunEventType::Finished) == 1 || sinkA.countOf(RunEventType::Failed) == 1);
    QVERIFY(sinkB.countOf(RunEventType::Finished) == 1 || sinkB.countOf(RunEventType::Failed) == 1);
}

void ScriptQueueTest::fifoSecondStartsAfterFirstTrailer()
{
    FakeScriptRunner runner;
    runner.holdRun = true;
    FakeOutputCapture capture;
    FakeEventSink sinkA;
    FakeEventSink sinkB;
    ScriptQueue queue(&runner, &capture);

    QTimer::singleShot(0, [&]() {
        // B enqueued while A held
        const RunHandle hb = queue.enqueue(RunRequest{QStringLiteral("b"), &sinkB});
        QCOMPARE(hb.queuePosition, 0); // next to run after A
        QVERIFY(sinkB.countOf(RunEventType::Queued) >= 1);
        QVERIFY(sinkB.countOf(RunEventType::Started) == 0);
        // A still active
        QCOMPARE(queue.activeJobId(), runner.calls.first().jobId);
        runner.holdRun = false;
        runner.release();
    });

    queue.enqueue(RunRequest{QStringLiteral("a"), &sinkA});

    // A trailer before B started is recorded in sink order:
    const int aFinishedIdx = [&]() {
        for (int i = 0; i < sinkA.events.size(); ++i) {
            if (ProtocolCodec::isTrailer(sinkA.events[i])) {
                return i;
            }
        }
        return -1;
    }();
    QVERIFY(aFinishedIdx >= 0);

    // B started only after A completed (second call after first released).
    QCOMPARE(runner.calls.size(), 2);
    QCOMPARE(runner.calls[0].scriptUtf8, QStringLiteral("a"));
    QCOMPARE(runner.calls[1].scriptUtf8, QStringLiteral("b"));
    QVERIFY(sinkB.countOf(RunEventType::Started) == 1);
    QVERIFY(sinkB.countOf(RunEventType::Finished) == 1);
}

void ScriptQueueTest::enqueueWhileRunningPosition()
{
    FakeScriptRunner runner;
    runner.holdRun = true;
    FakeOutputCapture capture;
    FakeEventSink sinkA;
    FakeEventSink sinkB;
    FakeEventSink sinkC;
    ScriptQueue queue(&runner, &capture);

    QTimer::singleShot(0, [&]() {
        const RunHandle b = queue.enqueue(RunRequest{QStringLiteral("b"), &sinkB});
        const RunHandle c = queue.enqueue(RunRequest{QStringLiteral("c"), &sinkC});
        QCOMPARE(b.queuePosition, 0);
        QCOMPARE(c.queuePosition, 1);
        QCOMPARE(sinkB.events.first().position.value_or(-1), 0);
        QCOMPARE(sinkC.events.first().position.value_or(-1), 1);
        QCOMPARE(queue.pendingCount(), 2);
        runner.holdRun = false;
        runner.release();
    });

    const RunHandle a = queue.enqueue(RunRequest{QStringLiteral("a"), &sinkA});
    QCOMPARE(a.queuePosition, 0);
    QCOMPARE(runner.calls.size(), 3);
}

void ScriptQueueTest::detachMidRunThenNextCompletes()
{
    FakeScriptRunner runner;
    runner.holdRun = true;
    FakeOutputCapture capture;
    FakeEventSink sinkA;
    FakeEventSink sinkB;
    ScriptQueue queue(&runner, &capture);

    QTimer::singleShot(0, [&]() {
        const QString activeId = queue.activeJobId();
        QVERIFY(!activeId.isEmpty());
        queue.onClientDetached(activeId);
        queue.enqueue(RunRequest{QStringLiteral("b"), &sinkB});
        runner.holdRun = false;
        runner.release();
    });

    queue.enqueue(RunRequest{QStringLiteral("a"), &sinkA});

    QCOMPARE(runner.maxConcurrent, 1);
    QCOMPARE(runner.calls.size(), 2);
    QVERIFY(sinkB.countOf(RunEventType::Finished) == 1);
    QCOMPARE(queue.activeJobId(), QString());
    QCOMPARE(queue.pendingCount(), 0);
}

void ScriptQueueTest::detachWhileQueuedDoesNotStartHost()
{
    FakeScriptRunner runner;
    runner.holdRun = true;
    FakeOutputCapture capture;
    FakeEventSink sinkA;
    FakeEventSink sinkB;
    FakeEventSink sinkC;
    ScriptQueue queue(&runner, &capture);

    QTimer::singleShot(0, [&]() {
        const RunHandle b = queue.enqueue(RunRequest{QStringLiteral("b"), &sinkB});
        queue.onClientDetached(b.id);
        queue.enqueue(RunRequest{QStringLiteral("c"), &sinkC});
        runner.holdRun = false;
        runner.release();
    });

    queue.enqueue(RunRequest{QStringLiteral("a"), &sinkA});

    // B never invoked on host
    for (const auto &call : runner.calls) {
        QVERIFY(call.scriptUtf8 != QStringLiteral("b"));
    }
    QVERIFY(runner.calls.size() == 2); // a and c only
    QVERIFY(sinkC.countOf(RunEventType::Finished) == 1);
    QCOMPARE(queue.activeJobId(), QString());
    QCOMPARE(queue.pendingCount(), 0);
}

void ScriptQueueTest::failedJobThenNextCompletes()
{
    FakeScriptRunner runner;
    runner.resultQueue = {
        RunResult{false, QStringLiteral("boom")},
        RunResult{true, {}},
    };
    FakeOutputCapture capture;
    FakeEventSink sinkA;
    FakeEventSink sinkB;
    ScriptQueue queue(&runner, &capture);

    queue.enqueue(RunRequest{QStringLiteral("bad"), &sinkA});
    queue.enqueue(RunRequest{QStringLiteral("good"), &sinkB});

    QCOMPARE(sinkA.countOf(RunEventType::Failed), 1);
    QCOMPARE(sinkA.events.last().error, QStringLiteral("boom"));
    QCOMPARE(sinkB.countOf(RunEventType::Finished), 1);
    QCOMPARE(queue.activeJobId(), QString());
}

void ScriptQueueTest::heartbeatUnderQuietHeldRun()
{
    FakeScriptRunner runner;
    runner.holdRun = true;
    FakeOutputCapture capture;
    FakeEventSink sink;
    ScriptQueue queue(&runner, &capture);
    queue.setHeartbeatIntervalMs(30);

    // Watchdog on the nested event loop: release once a heartbeat is seen.
    auto *watch = new QTimer(&queue);
    watch->setInterval(10);
    QObject::connect(watch, &QTimer::timeout, &queue, [watch, &sink, &runner]() {
        if (sink.countOf(RunEventType::Heartbeat) >= 1) {
            watch->stop();
            runner.release();
        }
    });
    watch->start();

    // Absolute safety cap so the suite cannot hang.
    QTimer::singleShot(3000, &queue, [&runner]() { runner.release(); });

    queue.enqueue(RunRequest{QStringLiteral("quiet"), &sink});

    watch->stop();

    QVERIFY2(sink.countOf(RunEventType::Heartbeat) >= 1,
             "expected at least one heartbeat while job held open");
    QVERIFY(sink.countOf(RunEventType::Finished) == 1);
}

void ScriptQueueTest::captureChunksBecomeStdoutStderr()
{
    FakeScriptRunner runner;
    FakeOutputCapture capture;
    FakeEventSink sink;
    ScriptQueue queue(&runner, &capture);

    // First job id is job-1 (serial starts at 1).
    capture.stopChunksByJob.insert(
        QStringLiteral("job-1"),
        {OutputChunk{OutputChunk::Stream::Stdout, QStringLiteral("out\n")},
         OutputChunk{OutputChunk::Stream::Stderr, QStringLiteral("err\n")}});

    queue.enqueue(RunRequest{QStringLiteral("io"), &sink});

    QCOMPARE(sink.countOf(RunEventType::Stdout), 1);
    QCOMPARE(sink.countOf(RunEventType::Stderr), 1);
    QVERIFY(sink.countOf(RunEventType::Finished) == 1);
}

void ScriptQueueTest::emptyScriptFailsWithoutRunner()
{
    FakeScriptRunner runner;
    FakeOutputCapture capture;
    FakeEventSink sink;
    ScriptQueue queue(&runner, &capture);

    queue.enqueue(RunRequest{QString(), &sink});

    QCOMPARE(runner.calls.size(), 0);
    QCOMPARE(sink.countOf(RunEventType::Failed), 1);
    QCOMPARE(sink.events.last().error, QStringLiteral("empty script body"));
}

QObject *createScriptQueueTest()
{
    return new ScriptQueueTest;
}

#include "script_queue_test.moc"

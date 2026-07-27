#include "FileTeeOutputBridge.h"
#include "ScriptQueue.h"

#include "fakes/FakeEventSink.h"
#include "fakes/FakeOutputCapture.h"
#include "fakes/FakeScriptRunner.h"

#include <QtTest>

#include <QFile>
#include <QTimer>

// FileTee + retention policy (seed 04). No cadwork / CwAPI3D.
class FileTeeCaptureTest : public QObject
{
    Q_OBJECT

private slots:
    void startCreatesCaptureFiles();
    void pollReturnsIncrementalStdoutStderr();
    void stopReturnsFinalChunksAndRemovesFiles();
    void prepareWrappedEntryRetainsScriptUntilStop();
    void fakeCapturePollPathEmitsStdoutStderrLog();
    void retentionFilesExistWhileCaptureActive();
};

void FileTeeCaptureTest::startCreatesCaptureFiles()
{
    FileTeeOutputBridge bridge;
    const CaptureHandles h = bridge.start(QStringLiteral("job-a"));

    QCOMPARE(h.jobId, QStringLiteral("job-a"));
    QVERIFY(!h.stdoutPath.isEmpty());
    QVERIFY(!h.stderrPath.isEmpty());
    QVERIFY(bridge.hasJob(QStringLiteral("job-a")));
    QVERIFY(bridge.captureFilesExist(QStringLiteral("job-a")));
    QVERIFY(QFile::exists(h.stdoutPath));
    QVERIFY(QFile::exists(h.stderrPath));

    bridge.stop(QStringLiteral("job-a"));
    QVERIFY(!bridge.hasJob(QStringLiteral("job-a")));
    QVERIFY(!QFile::exists(h.stdoutPath));
    QVERIFY(!QFile::exists(h.stderrPath));
}

void FileTeeCaptureTest::pollReturnsIncrementalStdoutStderr()
{
    FileTeeOutputBridge bridge;
    const CaptureHandles h = bridge.start(QStringLiteral("job-poll"));
    QVERIFY(bridge.captureFilesExist(QStringLiteral("job-poll")));

    {
        QFile out(h.stdoutPath);
        QVERIFY(out.open(QIODevice::Append));
        out.write("hello\n");
    }
    {
        QFile err(h.stderrPath);
        QVERIFY(err.open(QIODevice::Append));
        err.write("trace\n");
    }

    const QVector<OutputChunk> chunks = bridge.poll(QStringLiteral("job-poll"));
    QCOMPARE(chunks.size(), 2);
    QCOMPARE(chunks[0].stream, OutputChunk::Stream::Stdout);
    QCOMPARE(chunks[0].text, QStringLiteral("hello\n"));
    QCOMPARE(chunks[1].stream, OutputChunk::Stream::Stderr);
    QCOMPARE(chunks[1].text, QStringLiteral("trace\n"));

    // Second poll with no new bytes → empty.
    QCOMPARE(bridge.poll(QStringLiteral("job-poll")).size(), 0);

    {
        QFile out(h.stdoutPath);
        QVERIFY(out.open(QIODevice::Append));
        out.write("more");
    }
    const QVector<OutputChunk> more = bridge.poll(QStringLiteral("job-poll"));
    QCOMPARE(more.size(), 1);
    QCOMPARE(more[0].text, QStringLiteral("more"));

    bridge.stop(QStringLiteral("job-poll"));
}

void FileTeeCaptureTest::stopReturnsFinalChunksAndRemovesFiles()
{
    FileTeeOutputBridge bridge;
    const CaptureHandles h = bridge.start(QStringLiteral("job-stop"));
    {
        QFile out(h.stdoutPath);
        QVERIFY(out.open(QIODevice::Append));
        out.write("final-out");
    }

    const QVector<OutputChunk> finalChunks = bridge.stop(QStringLiteral("job-stop"));
    QCOMPARE(finalChunks.size(), 1);
    QCOMPARE(finalChunks[0].text, QStringLiteral("final-out"));
    QVERIFY(!bridge.hasJob(QStringLiteral("job-stop")));
    QVERIFY(!QFile::exists(h.stdoutPath));
    QVERIFY(!QFile::exists(h.stderrPath));
}

void FileTeeCaptureTest::prepareWrappedEntryRetainsScriptUntilStop()
{
    FileTeeOutputBridge bridge;
    bridge.start(QStringLiteral("job-wrap"));

    const QString wrapper = bridge.prepareWrappedEntry(
        QStringLiteral("job-wrap"),
        QStringLiteral("print('x')\n"));
    QVERIFY(!wrapper.isEmpty());
    QVERIFY(QFile::exists(wrapper));

    const QString userPath = bridge.userScriptPath(QStringLiteral("job-wrap"));
    QVERIFY(!userPath.isEmpty());
    QVERIFY(QFile::exists(userPath));

    {
        QFile user(userPath);
        QVERIFY(user.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(user.readAll()), QStringLiteral("print('x')\n"));
    }
    {
        QFile wrap(wrapper);
        QVERIFY(wrap.open(QIODevice::ReadOnly));
        const QString wrapBody = QString::fromUtf8(wrap.readAll());
        QVERIFY(wrapBody.contains(QStringLiteral("sys.stdout")));
        QVERIFY(wrapBody.contains(QStringLiteral("runpy.run_path")));
        QVERIFY(wrapBody.contains(QStringLiteral("_FileTee")));
    }

    // Retention: still present until stop (simulates mid-run host call window).
    QVERIFY(bridge.captureFilesExist(QStringLiteral("job-wrap")));
    QVERIFY(QFile::exists(wrapper));
    QVERIFY(QFile::exists(userPath));

    // Close handles before stop — Windows cannot remove open files.
    bridge.stop(QStringLiteral("job-wrap"));
    QVERIFY(!QFile::exists(wrapper));
    QVERIFY(!QFile::exists(userPath));
}

void FileTeeCaptureTest::fakeCapturePollPathEmitsStdoutStderrLog()
{
    // Architecture §5 fake path: poll/stop chunks become sink stdout/stderr; queue emits log.
    FakeScriptRunner runner;
    FakeOutputCapture capture;
    FakeEventSink sink;
    ScriptQueue queue(&runner, &capture);

    capture.pollChunksByJob.insert(
        QStringLiteral("job-1"),
        {OutputChunk{OutputChunk::Stream::Stdout, QStringLiteral("live-out")}});
    capture.stopChunksByJob.insert(
        QStringLiteral("job-1"),
        {OutputChunk{OutputChunk::Stream::Stderr, QStringLiteral("Traceback (most recent call last):")}});

    // Hold run so heartbeat can poll mid-job (stdout path while active).
    runner.holdRun = true;
    queue.setHeartbeatIntervalMs(20);

    QTimer::singleShot(80, &queue, [&runner]() {
        runner.holdRun = false;
        runner.release();
    });
    QTimer::singleShot(3000, &queue, [&runner]() { runner.release(); });

    queue.enqueue(RunRequest{QStringLiteral("print(1)"), &sink});

    QVERIFY(sink.countOf(RunEventType::Stdout) >= 1);
    QVERIFY(sink.countOf(RunEventType::Stderr) >= 1);
    QVERIFY(sink.countOf(RunEventType::Log) >= 1); // queue position diagnostics (US-7)
    QVERIFY(sink.countOf(RunEventType::Finished) == 1);

    bool sawLiveOut = false;
    bool sawTrace = false;
    bool sawQueueLog = false;
    for (const RunEvent &ev : sink.events) {
        if (ev.type == RunEventType::Stdout && ev.text.contains(QStringLiteral("live-out"))) {
            sawLiveOut = true;
        }
        if (ev.type == RunEventType::Stderr && ev.text.contains(QStringLiteral("Traceback"))) {
            sawTrace = true;
        }
        if (ev.type == RunEventType::Log && ev.text.contains(QStringLiteral("queue"))) {
            sawQueueLog = true;
        }
    }
    QVERIFY2(sawLiveOut, "expected stdout text live-out");
    QVERIFY2(sawTrace, "expected stderr traceback substring");
    QVERIFY2(sawQueueLog, "expected listener diagnostic log");
}

void FileTeeCaptureTest::retentionFilesExistWhileCaptureActive()
{
    // Scoped RAII proof: capture + script paths exist for the whole active window;
    // nothing is deleted until stop (mirrors Spec §9 / architecture temp retention).
    FileTeeOutputBridge bridge;
    const CaptureHandles h = bridge.start(QStringLiteral("job-retain"));
    const QString wrapper = bridge.prepareWrappedEntry(
        QStringLiteral("job-retain"),
        QStringLiteral("# body\n"));

    QVERIFY(bridge.hasJob(QStringLiteral("job-retain")));
    QVERIFY(QFile::exists(h.stdoutPath));
    QVERIFY(QFile::exists(h.stderrPath));
    QVERIFY(QFile::exists(wrapper));
    QVERIFY(QFile::exists(bridge.userScriptPath(QStringLiteral("job-retain"))));

    // Nested "host call" simulation: still-running → files must remain.
    {
        const QString entry = bridge.wrapperPath(QStringLiteral("job-retain"));
        QVERIFY(QFile::exists(entry));
        // poll must not remove files
        (void)bridge.poll(QStringLiteral("job-retain"));
        QVERIFY(QFile::exists(entry));
        QVERIFY(QFile::exists(h.stdoutPath));
    }

    bridge.stop(QStringLiteral("job-retain"));
    QVERIFY(!QFile::exists(h.stdoutPath));
    QVERIFY(!QFile::exists(wrapper));
}

QObject *createFileTeeCaptureTest()
{
    return new FileTeeCaptureTest;
}

#include "file_tee_capture_test.moc"

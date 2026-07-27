#include "ProtocolCodec.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString fixturesRoot()
{
#if defined(FIXTURES_DIR)
    return QStringLiteral(FIXTURES_DIR);
#else
    return QString();
#endif
}

QJsonObject parseObject(const QByteArray &line)
{
    return QJsonDocument::fromJson(line.trimmed()).object();
}

void assertJsonFieldEqual(const QJsonObject &expected, const QJsonObject &actual, const char *key)
{
    QCOMPARE(actual.value(QLatin1String(key)), expected.value(QLatin1String(key)));
}

} // namespace

class ProtocolCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void encodeMatchesHappyPathGoldenFields();
    void encodeMatchesFailedTrailerGoldenFields();
    void encodeMatchesStdoutStderrHeartbeatFields();
    void trailerClassification();
    void encodeEndsWithNewline();
    void decodeRoundTrip_data();
    void decodeRoundTrip();
};

void ProtocolCodecTest::encodeMatchesHappyPathGoldenFields()
{
    const QString path = QDir(fixturesRoot()).filePath(QStringLiteral("protocol/happy_path.ndjson"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QList<QByteArray> lines = file.readAll().split('\n');

    // Line 0: queued
    {
        RunEvent e;
        e.v = 1;
        e.type = RunEventType::Queued;
        e.jobId = QStringLiteral("job-happy-1");
        e.position = 0;
        e.ts = QStringLiteral("2026-01-15T10:00:00.000Z");
        const QJsonObject got = parseObject(ProtocolCodec::encode(e));
        const QJsonObject exp = parseObject(lines[0]);
        assertJsonFieldEqual(exp, got, "v");
        assertJsonFieldEqual(exp, got, "type");
        assertJsonFieldEqual(exp, got, "jobId");
        assertJsonFieldEqual(exp, got, "position");
        assertJsonFieldEqual(exp, got, "ts");
    }
    // Line 1: started
    {
        RunEvent e;
        e.type = RunEventType::Started;
        e.jobId = QStringLiteral("job-happy-1");
        e.ts = QStringLiteral("2026-01-15T10:00:00.050Z");
        const QJsonObject got = parseObject(ProtocolCodec::encode(e));
        const QJsonObject exp = parseObject(lines[1]);
        assertJsonFieldEqual(exp, got, "type");
        assertJsonFieldEqual(exp, got, "jobId");
        assertJsonFieldEqual(exp, got, "ts");
    }
    // Line 2: log
    {
        RunEvent e;
        e.type = RunEventType::Log;
        e.jobId = QStringLiteral("job-happy-1");
        e.text = QStringLiteral("queue position 0");
        e.level = QStringLiteral("info");
        e.ts = QStringLiteral("2026-01-15T10:00:00.060Z");
        const QJsonObject got = parseObject(ProtocolCodec::encode(e));
        const QJsonObject exp = parseObject(lines[2]);
        assertJsonFieldEqual(exp, got, "type");
        assertJsonFieldEqual(exp, got, "text");
        assertJsonFieldEqual(exp, got, "level");
    }
    // Line 3: finished trailer
    {
        RunEvent e;
        e.type = RunEventType::Finished;
        e.jobId = QStringLiteral("job-happy-1");
        e.durationMs = 42;
        e.ok = true;
        e.ts = QStringLiteral("2026-01-15T10:00:00.092Z");
        const QByteArray line = ProtocolCodec::encode(e);
        const QJsonObject got = parseObject(line);
        const QJsonObject exp = parseObject(lines[3]);
        assertJsonFieldEqual(exp, got, "type");
        assertJsonFieldEqual(exp, got, "durationMs");
        assertJsonFieldEqual(exp, got, "ok");
        QVERIFY(ProtocolCodec::isTrailerType(got.value(QStringLiteral("type")).toString()));
        QVERIFY(ProtocolCodec::isTrailer(e));
    }
}

void ProtocolCodecTest::encodeMatchesFailedTrailerGoldenFields()
{
    const QString path = QDir(fixturesRoot()).filePath(QStringLiteral("protocol/failed_trailer.ndjson"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QList<QByteArray> lines = file.readAll().split('\n');

    RunEvent e;
    e.type = RunEventType::Failed;
    e.jobId = QStringLiteral("job-fail-1");
    e.durationMs = 15;
    e.error = QStringLiteral("empty script body");
    e.ts = QStringLiteral("2026-01-15T10:01:00.025Z");
    const QJsonObject got = parseObject(ProtocolCodec::encode(e));
    const QJsonObject exp = parseObject(lines[3]);
    assertJsonFieldEqual(exp, got, "v");
    assertJsonFieldEqual(exp, got, "type");
    assertJsonFieldEqual(exp, got, "jobId");
    assertJsonFieldEqual(exp, got, "durationMs");
    assertJsonFieldEqual(exp, got, "error");
    QVERIFY(ProtocolCodec::isTrailer(e));
}

void ProtocolCodecTest::encodeMatchesStdoutStderrHeartbeatFields()
{
    const QString path = QDir(fixturesRoot()).filePath(QStringLiteral("protocol/with_stdout_stderr.ndjson"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QList<QByteArray> lines = file.readAll().split('\n');

    {
        RunEvent e;
        e.type = RunEventType::Stdout;
        e.jobId = QStringLiteral("job-io-1");
        e.text = QStringLiteral("hello\n");
        e.ts = QStringLiteral("2026-01-15T10:02:00.030Z");
        assertJsonFieldEqual(parseObject(lines[2]), parseObject(ProtocolCodec::encode(e)), "text");
        assertJsonFieldEqual(parseObject(lines[2]), parseObject(ProtocolCodec::encode(e)), "type");
    }
    {
        RunEvent e;
        e.type = RunEventType::Stderr;
        e.jobId = QStringLiteral("job-io-1");
        e.text = QStringLiteral(
            "Traceback (most recent call last):\n  File \"script.py\", line 1, in <module>\n"
            "    raise RuntimeError('boom')\nRuntimeError: boom\n");
        e.ts = QStringLiteral("2026-01-15T10:02:00.050Z");
        assertJsonFieldEqual(parseObject(lines[4]), parseObject(ProtocolCodec::encode(e)), "text");
        assertJsonFieldEqual(parseObject(lines[4]), parseObject(ProtocolCodec::encode(e)), "type");
    }
    {
        RunEvent e;
        e.type = RunEventType::Heartbeat;
        e.jobId = QStringLiteral("job-io-1");
        e.elapsedMs = 15000;
        e.ts = QStringLiteral("2026-01-15T10:02:15.020Z");
        assertJsonFieldEqual(parseObject(lines[5]), parseObject(ProtocolCodec::encode(e)), "elapsedMs");
        assertJsonFieldEqual(parseObject(lines[5]), parseObject(ProtocolCodec::encode(e)), "type");
    }
}

void ProtocolCodecTest::trailerClassification()
{
    RunEvent finished;
    finished.type = RunEventType::Finished;
    QVERIFY(ProtocolCodec::isTrailer(finished));
    QVERIFY(ProtocolCodec::isTrailerType(QStringLiteral("finished")));

    RunEvent failed;
    failed.type = RunEventType::Failed;
    QVERIFY(ProtocolCodec::isTrailer(failed));
    QVERIFY(ProtocolCodec::isTrailerType(QStringLiteral("failed")));

    RunEvent started;
    started.type = RunEventType::Started;
    QVERIFY(!ProtocolCodec::isTrailer(started));
    QVERIFY(!ProtocolCodec::isTrailerType(QStringLiteral("started")));
    QVERIFY(!ProtocolCodec::isTrailerType(QStringLiteral("heartbeat")));
    QVERIFY(!ProtocolCodec::isTrailerType(QStringLiteral("metric_tick")));
}

void ProtocolCodecTest::encodeEndsWithNewline()
{
    RunEvent e;
    e.type = RunEventType::Started;
    e.jobId = QStringLiteral("j");
    const QByteArray line = ProtocolCodec::encode(e);
    QVERIFY(line.endsWith('\n'));
    QCOMPARE(line.count('\n'), 1);
}

void ProtocolCodecTest::decodeRoundTrip_data()
{
    QTest::addColumn<int>("typeInt");
    QTest::addColumn<QString>("jobId");

    QTest::newRow("queued") << static_cast<int>(RunEventType::Queued) << QStringLiteral("a");
    QTest::newRow("finished") << static_cast<int>(RunEventType::Finished) << QStringLiteral("b");
    QTest::newRow("failed") << static_cast<int>(RunEventType::Failed) << QStringLiteral("c");
    QTest::newRow("heartbeat") << static_cast<int>(RunEventType::Heartbeat) << QStringLiteral("d");
}

void ProtocolCodecTest::decodeRoundTrip()
{
    QFETCH(int, typeInt);
    QFETCH(QString, jobId);

    RunEvent e;
    e.type = static_cast<RunEventType>(typeInt);
    e.jobId = jobId;
    e.v = 1;
    e.ts = QStringLiteral("2026-01-15T10:00:00.000Z");
    if (e.type == RunEventType::Queued) {
        e.position = 2;
    } else if (e.type == RunEventType::Finished) {
        e.durationMs = 10;
        e.ok = true;
    } else if (e.type == RunEventType::Failed) {
        e.durationMs = 3;
        e.error = QStringLiteral("x");
    } else if (e.type == RunEventType::Heartbeat) {
        e.elapsedMs = 15000;
    }

    const auto decoded = ProtocolCodec::decodeLine(ProtocolCodec::encode(e));
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->type, e.type);
    QCOMPARE(decoded->jobId, e.jobId);
    QCOMPARE(decoded->v, 1);
}

QObject *createProtocolCodecTest()
{
    return new ProtocolCodecTest;
}

#include "protocol_codec_test.moc"

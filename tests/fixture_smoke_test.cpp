#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

namespace {

QString fixturesRoot()
{
#if defined(FIXTURES_DIR)
    return QStringLiteral(FIXTURES_DIR);
#else
    return QString();
#endif
}

QString fixturePath(const QString &relative)
{
    return QDir(fixturesRoot()).filePath(relative);
}

bool isTrailerType(const QString &type)
{
    return type == QLatin1String("finished") || type == QLatin1String("failed");
}

} // namespace

class FixtureSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void fixturesRootConfigured();
    void protocolGoldensExistAndMatchSchema_data();
    void protocolGoldensExistAndMatchSchema();
    void scriptFixturesReadable_data();
    void scriptFixturesReadable();
};

void FixtureSmokeTest::fixturesRootConfigured()
{
    QVERIFY2(!fixturesRoot().isEmpty(), "FIXTURES_DIR compile definition missing");
    QVERIFY2(QDir(fixturesRoot()).exists(),
             qPrintable(QStringLiteral("fixtures root missing: %1").arg(fixturesRoot())));
}

void FixtureSmokeTest::protocolGoldensExistAndMatchSchema_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("expectedTrailer");

    QTest::newRow("happy_path") << QStringLiteral("happy_path.ndjson")
                                << QStringLiteral("finished");
    QTest::newRow("failed_trailer") << QStringLiteral("failed_trailer.ndjson")
                                    << QStringLiteral("failed");
    QTest::newRow("with_stdout_stderr") << QStringLiteral("with_stdout_stderr.ndjson")
                                        << QStringLiteral("finished");
    QTest::newRow("unknown_type") << QStringLiteral("unknown_type.ndjson")
                                  << QStringLiteral("finished");
}

void FixtureSmokeTest::protocolGoldensExistAndMatchSchema()
{
    QFETCH(QString, fileName);
    QFETCH(QString, expectedTrailer);

    const QString path = fixturePath(QStringLiteral("protocol/") + fileName);
    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(QStringLiteral("cannot open %1").arg(path)));

    const QByteArray content = file.readAll();
    QVERIFY2(!content.isEmpty(), qPrintable(QStringLiteral("empty golden: %1").arg(path)));

    const QList<QByteArray> lines = content.split('\n');
    int eventCount = 0;
    QString lastTrailer;

    for (const QByteArray &rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
        QVERIFY2(error.error == QJsonParseError::NoError,
                 qPrintable(QStringLiteral("%1 line parse: %2")
                                .arg(fileName, error.errorString())));
        QVERIFY2(doc.isObject(), qPrintable(QStringLiteral("%1: event is not an object").arg(fileName)));

        const QJsonObject obj = doc.object();
        QVERIFY2(obj.contains(QLatin1String("v")),
                 qPrintable(QStringLiteral("%1: missing v").arg(fileName)));
        QCOMPARE(obj.value(QLatin1String("v")).toInt(), 1);

        QVERIFY2(obj.contains(QLatin1String("type")),
                 qPrintable(QStringLiteral("%1: missing type").arg(fileName)));
        QVERIFY2(obj.contains(QLatin1String("jobId")),
                 qPrintable(QStringLiteral("%1: missing jobId").arg(fileName)));
        QVERIFY2(!obj.value(QLatin1String("jobId")).toString().isEmpty(),
                 qPrintable(QStringLiteral("%1: empty jobId").arg(fileName)));

        const QString type = obj.value(QLatin1String("type")).toString();
        if (type == QLatin1String("queued")) {
            QVERIFY2(obj.contains(QLatin1String("position")),
                     qPrintable(QStringLiteral("%1: queued missing position").arg(fileName)));
        } else if (type == QLatin1String("stdout") || type == QLatin1String("stderr")
                   || type == QLatin1String("log")) {
            QVERIFY2(obj.contains(QLatin1String("text")),
                     qPrintable(QStringLiteral("%1: %2 missing text").arg(fileName, type)));
        } else if (type == QLatin1String("finished")) {
            QVERIFY2(obj.contains(QLatin1String("durationMs")),
                     qPrintable(QStringLiteral("%1: finished missing durationMs").arg(fileName)));
            lastTrailer = type;
        } else if (type == QLatin1String("failed")) {
            QVERIFY2(obj.contains(QLatin1String("durationMs")),
                     qPrintable(QStringLiteral("%1: failed missing durationMs").arg(fileName)));
            QVERIFY2(obj.contains(QLatin1String("error")),
                     qPrintable(QStringLiteral("%1: failed missing error").arg(fileName)));
            lastTrailer = type;
        }

        ++eventCount;
    }

    QVERIFY2(eventCount > 0, qPrintable(QStringLiteral("%1: no events").arg(fileName)));
    QCOMPARE(lastTrailer, expectedTrailer);

    // Exactly one trailer ends the stream (architecture §2.4.1).
    int trailerCount = 0;
    for (const QByteArray &rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QJsonObject obj = QJsonDocument::fromJson(line).object();
        if (isTrailerType(obj.value(QLatin1String("type")).toString())) {
            ++trailerCount;
        }
    }
    QCOMPARE(trailerCount, 1);
}

void FixtureSmokeTest::scriptFixturesReadable_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<bool>("allowEmpty");
    QTest::addColumn<int>("minBytes");

    QTest::newRow("multiline_utf8") << QStringLiteral("multiline_utf8.py.txt") << false << 1;
    QTest::newRow("large_body") << QStringLiteral("large_body.py.txt") << false << (64 * 1024);
    QTest::newRow("empty_body") << QStringLiteral("empty_body.txt") << true << 0;
}

void FixtureSmokeTest::scriptFixturesReadable()
{
    QFETCH(QString, fileName);
    QFETCH(bool, allowEmpty);
    QFETCH(int, minBytes);

    const QString path = fixturePath(QStringLiteral("scripts/") + fileName);
    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly),
             qPrintable(QStringLiteral("cannot open %1").arg(path)));

    const QByteArray body = file.readAll();
    if (!allowEmpty) {
        QVERIFY2(!body.isEmpty(), qPrintable(QStringLiteral("empty fixture: %1").arg(path)));
    }
    QVERIFY2(body.size() >= minBytes,
             qPrintable(QStringLiteral("%1 size %2 < min %3")
                            .arg(fileName)
                            .arg(body.size())
                            .arg(minBytes)));

    // UTF-8 validity for non-empty script bodies.
    if (!body.isEmpty()) {
        const QString decoded = QString::fromUtf8(body);
        QVERIFY2(!decoded.isNull() || body.isEmpty(),
                 qPrintable(QStringLiteral("%1 is not valid UTF-8").arg(fileName)));
    }
}

QObject *createFixtureSmokeTest()
{
    return new FixtureSmokeTest;
}

#include "fixture_smoke_test.moc"

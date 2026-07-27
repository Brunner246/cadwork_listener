#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTextStream>

// US-17: README documents half-close + NDJSON + LocalHost; fire-and-forget not supported.
class ReadmeMarkersTest : public QObject
{
    Q_OBJECT

private slots:
    void readmeContainsProtocolMarkers();
};

namespace {

QString readmePath()
{
    // tests/ -> repo root
    return QDir(QStringLiteral(FIXTURES_DIR)).absoluteFilePath(QStringLiteral("../../README.md"));
}

QString readAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

} // namespace

void ReadmeMarkersTest::readmeContainsProtocolMarkers()
{
    const QString path = QDir::cleanPath(readmePath());
    QVERIFY2(QFile::exists(path), qPrintable(QStringLiteral("README missing at %1").arg(path)));

    const QString body = readAll(path);
    QVERIFY(!body.isEmpty());

    // Greppable US-17 markers (verification §2 / seed acceptance).
    QVERIFY2(body.contains(QStringLiteral("half-close"), Qt::CaseInsensitive)
                 || body.contains(QStringLiteral("Half-close")),
             "expected half-close marker");
    QVERIFY2(body.contains(QStringLiteral("NDJSON")), "expected NDJSON marker");
    QVERIFY2(body.contains(QStringLiteral("LocalHost"))
                 || body.contains(QStringLiteral("127.0.0.1")),
             "expected LocalHost / 127.0.0.1");
    QVERIFY2(body.contains(QStringLiteral("9999")), "expected port 9999");

    // Event types
    for (const char *type : {"queued", "started", "stdout", "stderr", "log", "heartbeat",
                             "finished", "failed"}) {
        QVERIFY2(body.contains(QLatin1String(type)),
                 qPrintable(QStringLiteral("expected event type %1").arg(QLatin1String(type))));
    }

    QVERIFY2(body.contains(QStringLiteral("supported contract"), Qt::CaseInsensitive)
                 || body.contains(QStringLiteral("Supported contract")),
             "expected supported contract wording");
    QVERIFY2(body.contains(QStringLiteral("fire-and-forget"), Qt::CaseInsensitive),
             "expected fire-and-forget mentioned as not supported");
    QVERIFY2(body.contains(QStringLiteral("not the supported contract"), Qt::CaseInsensitive)
                 || body.contains(QStringLiteral("unsupported"), Qt::CaseInsensitive),
             "expected fire-and-forget marked unsupported");
    QVERIFY2(body.contains(QStringLiteral("US-17")), "expected US-17 marker");
    QVERIFY2(body.contains(QStringLiteral("PowerShell"), Qt::CaseInsensitive),
             "expected PowerShell sample");
}

QObject *createReadmeMarkersTest()
{
    return new ReadmeMarkersTest;
}

#include "readme_markers_test.moc"

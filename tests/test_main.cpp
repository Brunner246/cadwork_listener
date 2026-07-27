#include <QtTest>

#include <QCoreApplication>

// Each translation unit defines a QObject test class; we run them from one main
// so the suite stays a single CTest executable (seed 01 harness).

class FixtureSmokeTest;
class ProtocolCodecTest;
class ScriptQueueTest;

// Forward declarations of factory helpers or we construct by including headers.
// Simpler: declare the test classes in their .cpp with Q_OBJECT and construct here
// via extern type — those types are only in .cpp TU. Use a registration approach:

using TestFactory = QObject *(*)();

// Factories provided by each test TU.
QObject *createFixtureSmokeTest();
QObject *createProtocolCodecTest();
QObject *createScriptQueueTest();

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);

    int status = 0;
    const TestFactory factories[] = {
        &createFixtureSmokeTest,
        &createProtocolCodecTest,
        &createScriptQueueTest,
    };

    for (TestFactory factory : factories) {
        QObject *testObject = factory();
        status |= QTest::qExec(testObject, argc, argv);
        delete testObject;
    }
    return status;
}

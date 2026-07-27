#ifndef LISTENER_FAKES_FAKE_SCRIPT_RUNNER_H
#define LISTENER_FAKES_FAKE_SCRIPT_RUNNER_H

#include "ports/ScriptRunner.h"

#include <QEventLoop>
#include <QString>
#include <QVector>
#include <utility>

// In-memory ScriptRunner (architecture §5): records payloads, hold/release for overlap tests.
class FakeScriptRunner final : public ScriptRunner
{
public:
    struct Call {
        QString scriptUtf8;
        QString jobId;
    };

    QVector<Call> calls;
    int currentConcurrent{0};
    int maxConcurrent{0};

    // When true, run() nests a QEventLoop until release() (timers can fire).
    bool holdRun{false};

    // Default result; can be overridden per call via resultQueue (FIFO).
    RunResult defaultResult{true, {}};
    QVector<RunResult> resultQueue;

    void release() const
    {
        if (heldLoop_ != nullptr) {
            heldLoop_->quit();
        }
    }

    RunResult run(const QString &scriptUtf8, const QString &jobId) override
    {
        calls.append(Call{scriptUtf8, jobId});
        ++currentConcurrent;
        if (currentConcurrent > maxConcurrent) {
            maxConcurrent = currentConcurrent;
        }

        if (holdRun) {
            QEventLoop loop;
            heldLoop_ = &loop;
            loop.exec();
            heldLoop_ = nullptr;
        }

        --currentConcurrent;

        if (!resultQueue.isEmpty()) {
            const RunResult r = resultQueue.front();
            resultQueue.removeFirst();
            return r;
        }
        return defaultResult;
    }

private:
    QEventLoop *heldLoop_{nullptr};
};

#endif // LISTENER_FAKES_FAKE_SCRIPT_RUNNER_H

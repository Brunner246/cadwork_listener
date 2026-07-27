#ifndef LISTENER_PORTS_SCRIPT_RUNNER_H
#define LISTENER_PORTS_SCRIPT_RUNNER_H

#include "RunEvent.h"

// Driven port (architecture §2.2): sync completion; return marks job end.
class ScriptRunner
{
public:
    virtual ~ScriptRunner() = default;

    virtual RunResult run(const QString &scriptUtf8, const QString &jobId) = 0;
};

#endif // LISTENER_PORTS_SCRIPT_RUNNER_H

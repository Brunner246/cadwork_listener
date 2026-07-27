//
// Created by MichaelBrunner on 22/05/2026.
//

#ifndef SCRIPTEXECUTOR_H
#define SCRIPTEXECUTOR_H

#include "ports/ScriptRunner.h"

#include <QObject>
#include <memory>
#include <vector>

namespace CwAPI3D::Interfaces
{
class ICwAPI3DUtilityController;
}

class ScriptFile;

// CwAPI3D ScriptRunner adapter (architecture §3.5). Temp retention hardening is seed 04.
class ScriptExecutor final : public QObject, public ScriptRunner
{
    Q_OBJECT

public:
    explicit ScriptExecutor(CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController,
                            QObject *parent = nullptr);
    ~ScriptExecutor() override;

    RunResult run(const QString &scriptUtf8, const QString &jobId) override;

private:
    CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController{nullptr};
    std::vector<std::unique_ptr<ScriptFile>> scripts;
};

#endif // SCRIPTEXECUTOR_H

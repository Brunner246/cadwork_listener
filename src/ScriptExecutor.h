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
class FileTeeOutputBridge;

// CwAPI3D ScriptRunner adapter (architecture §3.5 / seed 04).
// Retains temp script (and capture-owned wrapper) until run returns; capture stop cleans files.
class ScriptExecutor final : public QObject, public ScriptRunner
{
    Q_OBJECT

public:
    explicit ScriptExecutor(CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController,
                            FileTeeOutputBridge *capture = nullptr,
                            QObject *parent = nullptr);
    ~ScriptExecutor() override;

    RunResult run(const QString &scriptUtf8, const QString &jobId) override;

    // Test/diagnostic: true while host call is nested (retention window).
    [[nodiscard]] bool isHostCallActive() const { return hostCallActive_; }

    // Paths retained for the active host call (empty when idle). Unit-test seam.
    [[nodiscard]] QString activeEntryPath() const { return activeEntryPath_; }

private:
    CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController{nullptr};
    FileTeeOutputBridge *capture_{nullptr};
    // Fallback when no FileTee: hold raw script file for the duration of run() only.
    std::vector<std::unique_ptr<ScriptFile>> scripts;
    bool hostCallActive_{false};
    QString activeEntryPath_;
};

#endif // SCRIPTEXECUTOR_H

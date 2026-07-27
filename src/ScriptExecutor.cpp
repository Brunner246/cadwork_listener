//
// Created by MichaelBrunner on 22/05/2026.
//

#include "ScriptExecutor.h"
#include "FileTeeOutputBridge.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <cwapi3d/CwAPI3D.h>

// RAII temp script: lives until destructor — must outlive host call (Spec §9).
class ScriptFile
{
public:
    explicit ScriptFile(const QByteArray &content)
    {
        const QString tmpl = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("cw_script_XXXXXX.py"));
        file.setFileTemplate(tmpl);
        file.setAutoRemove(false);
        if (!file.open()) {
            qWarning() << "ScriptFile: failed to open temp file:" << file.errorString();
            return;
        }
        if (const qint64 written = file.write(content); written != content.size()) {
            qWarning() << "ScriptFile: short write" << written << "of" << content.size();
        }
        file.flush();
        filePath = file.fileName();
        file.close();
    }

    ~ScriptFile()
    {
        if (!filePath.isEmpty()) {
            QFile::remove(filePath);
        }
    }

    ScriptFile(const ScriptFile &) = delete;
    ScriptFile &operator=(const ScriptFile &) = delete;
    ScriptFile(ScriptFile &&) = delete;
    ScriptFile &operator=(ScriptFile &&) = delete;

    [[nodiscard]] const QString &path() const { return filePath; }
    [[nodiscard]] bool exists() const { return !filePath.isEmpty() && QFile::exists(filePath); }

private:
    QTemporaryFile file;
    QString filePath;
};

ScriptExecutor::ScriptExecutor(CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController,
                               FileTeeOutputBridge *capture,
                               QObject *parent)
    : QObject(parent),
      utilityController(utilityController),
      capture_(capture)
{
}

ScriptExecutor::~ScriptExecutor() = default;

RunResult ScriptExecutor::run(const QString &scriptUtf8, const QString &jobId)
{
    if (scriptUtf8.isEmpty()) {
        return RunResult{false, QStringLiteral("empty script body")};
    }
    if (utilityController == nullptr) {
        return RunResult{false, QStringLiteral("utility controller unavailable")};
    }

    // Hold temps for the full host call window, then release (Spec §9 / architecture §6).
    std::unique_ptr<ScriptFile> localScript;
    QString pathToRun;

    if (capture_ != nullptr) {
        pathToRun = capture_->prepareWrappedEntry(jobId, scriptUtf8);
        if (pathToRun.isEmpty()) {
            qWarning() << "ScriptExecutor: FileTee wrapper unavailable for" << jobId;
            return RunResult{false, QStringLiteral("capture wrapper unavailable")};
        }
    } else {
        localScript = std::make_unique<ScriptFile>(scriptUtf8.toUtf8());
        if (localScript->path().isEmpty()) {
            qWarning() << "ScriptExecutor: cannot run script, temp file unavailable";
            return RunResult{false, QStringLiteral("temp file unavailable")};
        }
        pathToRun = localScript->path();
        // Also keep on the vector for the duration of this call (explicit retention list).
        scripts.push_back(std::move(localScript));
    }

    hostCallActive_ = true;
    activeEntryPath_ = pathToRun;

    // Nested host call: do not delete pathToRun until after this returns.
    utilityController->runExternalProgramFromCustomDirectory(pathToRun.toStdWString().c_str());

    hostCallActive_ = false;
    activeEntryPath_.clear();

    // Release fallback script files only after host return (capture files cleaned in stop()).
    scripts.clear();

    return RunResult{true, {}};
}

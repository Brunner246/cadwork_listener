//
// Created by MichaelBrunner on 22/05/2026.
//

#include "ScriptExecutor.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <cwapi3d/CwAPI3D.h>

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

private:
    QTemporaryFile file;
    QString filePath;
};

ScriptExecutor::ScriptExecutor(CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController,
                               QObject *parent)
    : QObject(parent),
      utilityController(utilityController)
{
}

ScriptExecutor::~ScriptExecutor() = default;

RunResult ScriptExecutor::run(const QString &scriptUtf8, const QString & /*jobId*/)
{
    if (scriptUtf8.isEmpty()) {
        return RunResult{false, QStringLiteral("empty script body")};
    }
    if (utilityController == nullptr) {
        return RunResult{false, QStringLiteral("utility controller unavailable")};
    }

    const QByteArray content = scriptUtf8.toUtf8();
    auto scriptFile = std::make_unique<ScriptFile>(content);
    if (scriptFile->path().isEmpty()) {
        qWarning() << "ScriptExecutor: cannot run script, temp file unavailable";
        return RunResult{false, QStringLiteral("temp file unavailable")};
    }
    const QString path = scriptFile->path();
    scripts.push_back(std::move(scriptFile));
    utilityController->runExternalProgramFromCustomDirectory(path.toStdWString().c_str());
    return RunResult{true, {}};
}

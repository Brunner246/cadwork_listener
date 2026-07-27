#ifndef LISTENER_FILE_TEE_OUTPUT_BRIDGE_H
#define LISTENER_FILE_TEE_OUTPUT_BRIDGE_H

#include "ports/OutputCapture.h"

#include <QHash>
#include <QString>

// OutputCapture adapter (architecture §3.4): job-scoped file tee of Python stdout/stderr.
// Real host fidelity is deferred Manual HITL; poll/stop path is unit-testable headlessly.
class FileTeeOutputBridge final : public OutputCapture
{
public:
    CaptureHandles start(const QString &jobId) override;
    QVector<OutputChunk> poll(const QString &jobId) override;
    QVector<OutputChunk> stop(const QString &jobId) override;

    // Build wrapper entry that tees sys.stdout/sys.stderr then runs user script (runpy).
    // Requires start(jobId) first. Returns absolute path to the wrapper .py, or empty on failure.
    [[nodiscard]] QString prepareWrappedEntry(const QString &jobId, const QString &userScriptUtf8);

    [[nodiscard]] QString stdoutPath(const QString &jobId) const;
    [[nodiscard]] QString stderrPath(const QString &jobId) const;
    [[nodiscard]] QString wrapperPath(const QString &jobId) const;
    [[nodiscard]] QString userScriptPath(const QString &jobId) const;
    [[nodiscard]] bool hasJob(const QString &jobId) const;
    [[nodiscard]] bool captureFilesExist(const QString &jobId) const;

private:
    struct JobState {
        QString stdoutPath;
        QString stderrPath;
        QString userScriptPath;
        QString wrapperPath;
        qint64 stdoutOffset{0};
        qint64 stderrOffset{0};
        bool stopped{false};
    };

    QHash<QString, JobState> jobs_;

    QVector<OutputChunk> readDelta(JobState &state);
    void removeJobFiles(JobState &state) const;
    static QString makeTempPath(const QString &prefix, const QString &suffix);
    static bool writeAll(const QString &path, const QByteArray &bytes);
    static QString pythonStringLiteral(const QString &path);
};

#endif // LISTENER_FILE_TEE_OUTPUT_BRIDGE_H

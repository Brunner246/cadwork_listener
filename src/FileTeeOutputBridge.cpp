#include "FileTeeOutputBridge.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>

namespace {

QString tempDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

} // namespace

QString FileTeeOutputBridge::makeTempPath(const QString &prefix, const QString &suffix)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return QDir(tempDir()).filePath(prefix + id + suffix);
}

bool FileTeeOutputBridge::writeAll(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "FileTeeOutputBridge: cannot write" << path << f.errorString();
        return false;
    }
    if (f.write(bytes) != bytes.size()) {
        qWarning() << "FileTeeOutputBridge: short write" << path;
        return false;
    }
    f.flush();
    return true;
}

QString FileTeeOutputBridge::pythonStringLiteral(const QString &path)
{
    // Embed Windows/POSIX paths as raw Python string with escaped backslashes and quotes.
    QString escaped = path;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    return QStringLiteral("'%1'").arg(escaped);
}

CaptureHandles FileTeeOutputBridge::start(const QString &jobId)
{
    CaptureHandles handles;
    handles.jobId = jobId;

    if (jobId.isEmpty()) {
        return handles;
    }

    // Replace any prior capture for this id (should not happen under single-active queue).
    if (jobs_.contains(jobId)) {
        removeJobFiles(jobs_[jobId]);
        jobs_.remove(jobId);
    }

    JobState state;
    state.stdoutPath = makeTempPath(QStringLiteral("cw_cap_stdout_"), QStringLiteral(".txt"));
    state.stderrPath = makeTempPath(QStringLiteral("cw_cap_stderr_"), QStringLiteral(".txt"));

    // Create empty files so poll can open them before Python writes.
    if (!writeAll(state.stdoutPath, {}) || !writeAll(state.stderrPath, {})) {
        removeJobFiles(state);
        return handles;
    }

    handles.stdoutPath = state.stdoutPath;
    handles.stderrPath = state.stderrPath;
    jobs_.insert(jobId, state);
    return handles;
}

QVector<OutputChunk> FileTeeOutputBridge::poll(const QString &jobId)
{
    auto it = jobs_.find(jobId);
    if (it == jobs_.end() || it->stopped) {
        return {};
    }
    return readDelta(*it);
}

QVector<OutputChunk> FileTeeOutputBridge::stop(const QString &jobId)
{
    auto it = jobs_.find(jobId);
    if (it == jobs_.end()) {
        return {};
    }

    QVector<OutputChunk> finalChunks = readDelta(*it);
    it->stopped = true;
    removeJobFiles(*it);
    jobs_.erase(it);
    return finalChunks;
}

QString FileTeeOutputBridge::prepareWrappedEntry(const QString &jobId, const QString &userScriptUtf8)
{
    auto it = jobs_.find(jobId);
    if (it == jobs_.end() || it->stopped) {
        qWarning() << "FileTeeOutputBridge: prepareWrappedEntry without active start for" << jobId;
        return {};
    }

    JobState &state = *it;

    // User body as a separate file (large scripts; Spec US-12).
    if (state.userScriptPath.isEmpty()) {
        state.userScriptPath = makeTempPath(QStringLiteral("cw_script_"), QStringLiteral(".py"));
    }
    if (!writeAll(state.userScriptPath, userScriptUtf8.toUtf8())) {
        return {};
    }

    // Wrapper: application-level tee of sys.stdout/sys.stderr (architecture §3.4), then run user file.
    const QString wrapper = QStringLiteral(
                                "# -*- coding: utf-8 -*-\n"
                                "# cadwork_listener FileTeeOutputBridge wrapper (protocol capture)\n"
                                "import sys\n"
                                "import runpy\n"
                                "\n"
                                "class _FileTee:\n"
                                "    def __init__(self, path, original):\n"
                                "        self._f = open(path, 'a', encoding='utf-8', buffering=1, errors='replace')\n"
                                "        self._orig = original\n"
                                "    def write(self, s):\n"
                                "        try:\n"
                                "            self._f.write(s)\n"
                                "            self._f.flush()\n"
                                "        except Exception:\n"
                                "            pass\n"
                                "        if self._orig is not None:\n"
                                "            try:\n"
                                "                self._orig.write(s)\n"
                                "            except Exception:\n"
                                "                pass\n"
                                "        return len(s) if isinstance(s, str) else 0\n"
                                "    def flush(self):\n"
                                "        try:\n"
                                "            self._f.flush()\n"
                                "        except Exception:\n"
                                "            pass\n"
                                "        if self._orig is not None:\n"
                                "            try:\n"
                                "                self._orig.flush()\n"
                                "            except Exception:\n"
                                "                pass\n"
                                "    def fileno(self):\n"
                                "        return self._f.fileno()\n"
                                "    def isatty(self):\n"
                                "        return False\n"
                                "\n"
                                "_stdout_path = %1\n"
                                "_stderr_path = %2\n"
                                "sys.stdout = _FileTee(_stdout_path, getattr(sys, 'stdout', None))\n"
                                "sys.stderr = _FileTee(_stderr_path, getattr(sys, 'stderr', None))\n"
                                "runpy.run_path(%3, run_name='__main__')\n")
                                .arg(pythonStringLiteral(state.stdoutPath),
                                     pythonStringLiteral(state.stderrPath),
                                     pythonStringLiteral(state.userScriptPath));

    if (state.wrapperPath.isEmpty()) {
        state.wrapperPath = makeTempPath(QStringLiteral("cw_wrap_"), QStringLiteral(".py"));
    }
    if (!writeAll(state.wrapperPath, wrapper.toUtf8())) {
        return {};
    }
    return state.wrapperPath;
}

QString FileTeeOutputBridge::stdoutPath(const QString &jobId) const
{
    const auto it = jobs_.constFind(jobId);
    return it == jobs_.cend() ? QString() : it->stdoutPath;
}

QString FileTeeOutputBridge::stderrPath(const QString &jobId) const
{
    const auto it = jobs_.constFind(jobId);
    return it == jobs_.cend() ? QString() : it->stderrPath;
}

QString FileTeeOutputBridge::wrapperPath(const QString &jobId) const
{
    const auto it = jobs_.constFind(jobId);
    return it == jobs_.cend() ? QString() : it->wrapperPath;
}

QString FileTeeOutputBridge::userScriptPath(const QString &jobId) const
{
    const auto it = jobs_.constFind(jobId);
    return it == jobs_.cend() ? QString() : it->userScriptPath;
}

bool FileTeeOutputBridge::hasJob(const QString &jobId) const
{
    return jobs_.contains(jobId);
}

bool FileTeeOutputBridge::captureFilesExist(const QString &jobId) const
{
    const auto it = jobs_.constFind(jobId);
    if (it == jobs_.cend()) {
        return false;
    }
    return QFile::exists(it->stdoutPath) && QFile::exists(it->stderrPath);
}

QVector<OutputChunk> FileTeeOutputBridge::readDelta(JobState &state)
{
    QVector<OutputChunk> chunks;

    auto readStream = [&](const QString &path, qint64 &offset, const OutputChunk::Stream stream) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            return;
        }
        if (offset > f.size()) {
            offset = f.size();
        }
        if (!f.seek(offset)) {
            return;
        }
        const QByteArray data = f.readAll();
        offset = f.pos();
        if (data.isEmpty()) {
            return;
        }
        OutputChunk chunk;
        chunk.stream = stream;
        chunk.text = QString::fromUtf8(data);
        chunks.append(chunk);
    };

    readStream(state.stdoutPath, state.stdoutOffset, OutputChunk::Stream::Stdout);
    readStream(state.stderrPath, state.stderrOffset, OutputChunk::Stream::Stderr);
    return chunks;
}

void FileTeeOutputBridge::removeJobFiles(JobState &state) const
{
    auto removePath = [](QString &path) {
        if (!path.isEmpty()) {
            QFile::remove(path);
            path.clear();
        }
    };
    removePath(state.stdoutPath);
    removePath(state.stderrPath);
    removePath(state.userScriptPath);
    removePath(state.wrapperPath);
}

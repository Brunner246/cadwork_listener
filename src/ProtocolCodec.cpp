#include "ProtocolCodec.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace {

QJsonObject toJson(const RunEvent &event)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("v"), event.v);
    obj.insert(QStringLiteral("type"), ProtocolCodec::typeToString(event.type));
    obj.insert(QStringLiteral("jobId"), event.jobId);
    if (!event.ts.isEmpty()) {
        obj.insert(QStringLiteral("ts"), event.ts);
    }

    switch (event.type) {
    case RunEventType::Queued:
        if (event.position.has_value()) {
            obj.insert(QStringLiteral("position"), *event.position);
        }
        break;
    case RunEventType::Stdout:
    case RunEventType::Stderr:
        obj.insert(QStringLiteral("text"), event.text);
        break;
    case RunEventType::Log:
        obj.insert(QStringLiteral("text"), event.text);
        if (!event.level.isEmpty()) {
            obj.insert(QStringLiteral("level"), event.level);
        }
        break;
    case RunEventType::Heartbeat:
        if (event.elapsedMs.has_value()) {
            obj.insert(QStringLiteral("elapsedMs"), static_cast<qint64>(*event.elapsedMs));
        }
        break;
    case RunEventType::Finished:
        if (event.durationMs.has_value()) {
            obj.insert(QStringLiteral("durationMs"), static_cast<qint64>(*event.durationMs));
        }
        if (event.ok.has_value()) {
            obj.insert(QStringLiteral("ok"), *event.ok);
        }
        break;
    case RunEventType::Failed:
        if (event.durationMs.has_value()) {
            obj.insert(QStringLiteral("durationMs"), static_cast<qint64>(*event.durationMs));
        }
        obj.insert(QStringLiteral("error"), event.error);
        break;
    case RunEventType::Started:
        break;
    }
    return obj;
}

std::optional<RunEvent> fromJson(const QJsonObject &obj)
{
    if (!obj.contains(QStringLiteral("type")) || !obj.contains(QStringLiteral("jobId"))) {
        return std::nullopt;
    }
    const auto type = ProtocolCodec::typeFromString(obj.value(QStringLiteral("type")).toString());
    if (!type.has_value()) {
        return std::nullopt;
    }

    RunEvent event;
    event.v = obj.value(QStringLiteral("v")).toInt(1);
    event.type = *type;
    event.jobId = obj.value(QStringLiteral("jobId")).toString();
    event.ts = obj.value(QStringLiteral("ts")).toString();

    if (obj.contains(QStringLiteral("position"))) {
        event.position = obj.value(QStringLiteral("position")).toInt();
    }
    if (obj.contains(QStringLiteral("text"))) {
        event.text = obj.value(QStringLiteral("text")).toString();
    }
    if (obj.contains(QStringLiteral("level"))) {
        event.level = obj.value(QStringLiteral("level")).toString();
    }
    if (obj.contains(QStringLiteral("elapsedMs"))) {
        event.elapsedMs = static_cast<qint64>(obj.value(QStringLiteral("elapsedMs")).toDouble());
    }
    if (obj.contains(QStringLiteral("durationMs"))) {
        event.durationMs = static_cast<qint64>(obj.value(QStringLiteral("durationMs")).toDouble());
    }
    if (obj.contains(QStringLiteral("ok"))) {
        event.ok = obj.value(QStringLiteral("ok")).toBool();
    }
    if (obj.contains(QStringLiteral("error"))) {
        event.error = obj.value(QStringLiteral("error")).toString();
    }
    return event;
}

} // namespace

QByteArray ProtocolCodec::encode(const RunEvent &event)
{
    const QJsonDocument doc(toJson(event));
    QByteArray line = doc.toJson(QJsonDocument::Compact);
    line.append('\n');
    return line;
}

std::optional<RunEvent> ProtocolCodec::decodeLine(const QByteArray &line)
{
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    return fromJson(doc.object());
}

bool ProtocolCodec::isTrailer(const RunEvent &event)
{
    return event.type == RunEventType::Finished || event.type == RunEventType::Failed;
}

bool ProtocolCodec::isTrailerType(const QString &type)
{
    return type == QLatin1String("finished") || type == QLatin1String("failed");
}

QString ProtocolCodec::typeToString(const RunEventType type)
{
    switch (type) {
    case RunEventType::Queued:
        return QStringLiteral("queued");
    case RunEventType::Started:
        return QStringLiteral("started");
    case RunEventType::Stdout:
        return QStringLiteral("stdout");
    case RunEventType::Stderr:
        return QStringLiteral("stderr");
    case RunEventType::Log:
        return QStringLiteral("log");
    case RunEventType::Heartbeat:
        return QStringLiteral("heartbeat");
    case RunEventType::Finished:
        return QStringLiteral("finished");
    case RunEventType::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("log");
}

std::optional<RunEventType> ProtocolCodec::typeFromString(const QString &type)
{
    if (type == QLatin1String("queued")) {
        return RunEventType::Queued;
    }
    if (type == QLatin1String("started")) {
        return RunEventType::Started;
    }
    if (type == QLatin1String("stdout")) {
        return RunEventType::Stdout;
    }
    if (type == QLatin1String("stderr")) {
        return RunEventType::Stderr;
    }
    if (type == QLatin1String("log")) {
        return RunEventType::Log;
    }
    if (type == QLatin1String("heartbeat")) {
        return RunEventType::Heartbeat;
    }
    if (type == QLatin1String("finished")) {
        return RunEventType::Finished;
    }
    if (type == QLatin1String("failed")) {
        return RunEventType::Failed;
    }
    return std::nullopt;
}

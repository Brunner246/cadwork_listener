#ifndef LISTENER_PROTOCOL_CODEC_H
#define LISTENER_PROTOCOL_CODEC_H

#include "ports/RunEvent.h"

#include <QByteArray>
#include <QString>
#include <optional>

// Deep module (architecture §3.2): RunEvent ↔ NDJSON line with trailing \n.
class ProtocolCodec
{
public:
    // Single UTF-8 NDJSON line including terminating '\n'.
    [[nodiscard]] static QByteArray encode(const RunEvent &event);

    // Parse one line (with or without trailing '\n'). Empty / invalid → nullopt.
    [[nodiscard]] static std::optional<RunEvent> decodeLine(const QByteArray &line);

    [[nodiscard]] static bool isTrailer(const RunEvent &event);
    [[nodiscard]] static bool isTrailerType(const QString &type);

    [[nodiscard]] static QString typeToString(RunEventType type);
    [[nodiscard]] static std::optional<RunEventType> typeFromString(const QString &type);
};

#endif // LISTENER_PROTOCOL_CODEC_H

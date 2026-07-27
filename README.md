# cadwork_listener

Qt/CwAPI3D plugin that accepts Python from a local TCP client (VS Code **cadwork-python-runner** or PowerShell), runs it **one job at a time** in the open cadwork session, and streams run events back on the **same connection**.

## Supported contract (protocol v1)

> **US-17:** This README documents the **supported** wire contract.  
> **Fire-and-forget-only is not the supported contract.** Old clients that write bytes, fully close, and expect no reply may still enqueue via the orphan path, but that mode is **unsupported**.

| Item | Value |
|------|--------|
| Default bind | **LocalHost** (`127.0.0.1`), port **9999** |
| Request | Raw **UTF-8** script body, then **half-close** the write side (TCP FIN / `socket.end()` after write). Do **not** destroy the socket before reading the reply stream. |
| Response | **NDJSON** event stream: one UTF-8 JSON object per line, `\n`-terminated, until a **trailer** |

### Event types (`v:1`)

Common fields: `{"v":1,"type":"<type>","jobId":"<id>","ts":"<iso8601-optional>"}`.

| `type` | Role | Extra fields |
|--------|------|----------------|
| `queued` | Accepted onto FIFO | `position` (0 = next to run) |
| `started` | About to enter host run | — |
| `stdout` | Captured Python stdout chunk | `text` |
| `stderr` | Captured Python stderr / traceback | `text` |
| `log` | Per-run listener diagnostics | `text`, optional `level` |
| `heartbeat` | Quiet-run tick (~15s when Qt timers fire) | optional `elapsedMs` |
| `finished` | **Trailer** (success) | `durationMs`, optional `ok` |
| `failed` | **Trailer** (failure) | `durationMs`, `error` |

Exactly one of `finished` | `failed` ends the stream. Empty body → immediate `failed`.

### Concurrency

- Exactly **one** script executes in cadwork at a time (`ScriptQueue` FIFO).
- A second client may connect and enqueue while the first run is active.
- Client disconnect mid-run does not cancel the host job; the queue continues (orphan → null sink).

### Output capture

Application-level **file tee** of `sys.stdout` / `sys.stderr` (not host-logger-only). Live mid-run flush is **best-effort** when the Qt event loop can process timers during host execution; the **trailer is always** required after the host call returns.

## PowerShell sample (half-close + NDJSON)

```powershell
# US-17 PowerShell one-shot: half-close request, read NDJSON until trailer.
$script = @'
print("hello from cadwork_listener")
'@

$client = [System.Net.Sockets.TcpClient]::new()
$client.Connect("127.0.0.1", 9999)
$stream = $client.GetStream()

$bytes = [System.Text.Encoding]::UTF8.GetBytes($script)
$stream.Write($bytes, 0, $bytes.Length)
# Half-close write side; keep reading the NDJSON event stream.
$stream.Socket.Shutdown([System.Net.Sockets.SocketShutdown]::Send)

$reader = New-Object System.IO.StreamReader($stream, [System.Text.Encoding]::UTF8)
while ($null -ne ($line = $reader.ReadLine())) {
    Write-Host $line
    if ($line -match '"type"\s*:\s*"(finished|failed)"') { break }
}

$reader.Close()
$client.Close()
```

## Build

Use the CMake presets in this repo (e.g. `local-relwithdebinfo`). Plugin target: `cadwork_listener`. Headless unit tests: `listener_unit_tests` via `ctest`.

```text
cmake --preset local-relwithdebinfo
cmake --build --preset local-relwithdebinfo
ctest --test-dir cmake-build-local-relwithdebinfo --output-on-failure
```

## Scope notes

- **In scope:** FIFO queue, same-connection NDJSON stream, stdout/stderr/log/heartbeat, LocalHost bind, README contract.
- **Out of scope:** cancel-in-flight, multi-user auth, full cadwork UI/logger mirror, marketplace packaging.

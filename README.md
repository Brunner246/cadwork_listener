# cadwork_listener

A cadwork 3D plugin that exposes a local TCP listener so external tools (e.g. a VS Code extension) can push Python
scripts into a running cadwork session for execution against the CwAPI3D.

## How it works

The plugin is loaded by cadwork via `plugin_x64_init` (see `src/library.cpp`) and starts a Qt TCP server on
`127.0.0.1:9999`.

- `ServerHandler` owns the `QTcpServer` and a `ScriptExecutor`, and wires each new connection to a `ClientSession`.
- `ClientSession` buffers all bytes received on the socket until the client disconnects, then emits the complete payload
  as `scriptReceived`.
- `ScriptExecutor` writes the received bytes to a unique temp file (`cw_script_XXXXXX.py` under the system temp dir) and
  invokes `ICwAPI3DUtilityController::runExternalProgramFromCustomDirectory` on it. The temp file is cleaned up when the
  `ScriptFile` RAII wrapper goes out of scope.

The protocol is intentionally minimal: open the socket, send the script bytes, close. No framing, no response.

`src/cadwork_listener.py` is a standalone PyQt6 prototype of the same server, runnable outside cadwork for
experimentation.

## Requirements

- Windows, MSVC (cl), Ninja
- CMake >= 3.30, C++20
- Qt 6 (Core, Network, Widgets)
- CwAPI3D (via `find_package(CwAPI3D CONFIG REQUIRED)`)
- vcpkg (used as the toolchain in `CMakeUserPresets.json`)

## Build

The repo ships with `CMakePresets.json` (compiler + build-type presets) and `CMakeUserPresets.json` (local paths to Qt
and the vcpkg toolchain — adjust to your machine).

```powershell
cmake --preset local-relwithdebinfo
cmake --build --preset local-relwithdebinfo
```

Other presets: `local-release`, `local-debug`. The build produces `cadwork_listener.dll`.

## Install

Copy the resulting `cadwork_listener.dll` into cadwork's plugin directory. Plugin metadata is set via the compile
definitions in the top-level `CMakeLists.txt`:

- `CWAPI3D_PLUGIN_NAME = "VS Code Listener"`
- `CWAPI3D_AUTHOR_NAME = "Michael Brunner"`
- `CWAPI3D_AUTHOR_EMAIL = "brunner@cadwork.swiss"`

## Usage

With cadwork running and the plugin loaded, send a Python script to the listener:

```powershell
$client = New-Object System.Net.Sockets.TcpClient('127.0.0.1', 9999)
$stream = $client.GetStream()
$bytes = [System.IO.File]::ReadAllBytes('C:\path\to\script.py')
$stream.Write($bytes, 0, $bytes.Length)
$client.Close()
```

The script runs in cadwork via the utility controller and has full access to the active model through CwAPI3D.

## Layout

```
src/
  library.cpp          # plugin entry point (plugin_x64_init)
  ServerHandler.{h,cpp}  # QTcpServer owner, accepts connections
  ClientSession.{h,cpp}  # per-connection buffer; emits on disconnect
  ScriptExecutor.{h,cpp} # writes script to temp file, runs via CwAPI3D
  cadwork_listener.py    # standalone PyQt6 reference implementation
```

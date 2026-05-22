from PyQt6.QtCore import QObject, pyqtSignal, QEventLoop, QStandardPaths
from PyQt6.QtNetwork import QAbstractSocket, QTcpServer, QTcpSocket, QHostAddress
import os
import tempfile
import utility_controller as uc


class ScriptFile:
    def __init__(self, content: bytes):
        temp_dir = QStandardPaths.writableLocation(
            QStandardPaths.StandardLocation.TempLocation)
        fd, self._path = tempfile.mkstemp(
            prefix="cw_script_", suffix=".py", dir=temp_dir)
        try:
            with os.fdopen(fd, "wb") as f:
                f.write(content)
        except Exception:
            self._remove()
            raise

    @property
    def path(self) -> str:
        return self._path

    def _remove(self):
        try:
            if self._path and os.path.exists(self._path):
                os.remove(self._path)
        except OSError as e:
            print(f"ScriptFile: failed to remove {self._path}: {e}")

    def __del__(self):
        self._remove()


class ScriptExecutor(QObject):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._scripts: list[ScriptFile] = []

    def execute_script(self, script: bytes):
        if not script:
            return
        try:
            script_file = ScriptFile(script)
        except OSError as e:
            print(f"ScriptExecutor: cannot create temp file: {e}")
            return
        self._scripts.append(script_file)
        uc.run_external_program_from_custom_directory(script_file.path)


class ClientSession(QObject):
    scriptReceived = pyqtSignal(bytes)

    def __init__(self, socket: QTcpSocket, parent=None):
        super().__init__(parent)
        self._socket = socket
        self._socket.setParent(self)
        self._buffer = bytearray()
        self._closed = False
        self._socket.readyRead.connect(self._on_ready_read)
        self._socket.disconnected.connect(self._on_disconnected)
        self._socket.errorOccurred.connect(self._on_error)

    def _on_ready_read(self):
        self._buffer.extend(bytes(self._socket.readAll()))

    def _on_disconnected(self):
        if self._closed:
            return
        self._closed = True
        self._buffer.extend(bytes(self._socket.readAll()))
        if self._buffer:
            self.scriptReceived.emit(bytes(self._buffer))
        print("Client Disconnected")
        self.deleteLater()

    def _on_error(self):
        if self._socket.error() != QAbstractSocket.SocketError.RemoteHostClosedError:
            print(f"ClientSession error: {self._socket.errorString()}")


class ServerHandler(QObject):
    serverStopped = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._server = QTcpServer(self)
        self._executor = ScriptExecutor(self)
        self._server.newConnection.connect(self._handle_new_connection)
        if not self._server.listen(QHostAddress.Any, 9999):
            print(f"Server failed to start: {self._server.errorString()}")
        else:
            print("Server listening on port 9999...")

    def run_event_loop(self):
        loop = QEventLoop()
        self.serverStopped.connect(loop.quit)
        loop.exec()

    def _handle_new_connection(self):
        socket = self._server.nextPendingConnection()
        if socket is None:
            return
        print("Client Connected!")
        session = ClientSession(socket, self)
        session.scriptReceived.connect(self._executor.execute_script)


if __name__ == '__main__':
    server_handler = ServerHandler()
    server_handler.run_event_loop()

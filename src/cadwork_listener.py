from PyQt5.QtCore import QObject, pyqtSignal, QEventLoop, QStandardPaths, QIODevice
from PyQt5.QtNetwork import QTcpServer, QTcpSocket, QHostAddress
import os
import utility_controller as uc


class ServerHandler(QObject):
    serverStopped = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.server = QTcpServer(self)
        self.server.newConnection.connect(self.handle_new_connection)

        if not self.server.listen(QHostAddress.Any, 9999):
            print(f"Server failed to start: {self.server.errorString()}")
        else:
            print("Server listening on port 9999...")

    def run_event_loop(self):
        """Start the event loop to keep the server running."""
        loop = QEventLoop()
        self.serverStopped.connect(loop.quit)
        loop.exec()

    def handle_new_connection(self):
        client_socket = self.server.nextPendingConnection()
        print("Client Connected!")

        client_socket.disconnected.connect(
            lambda: self.handle_disconnection(client_socket))
        client_socket.readyRead.connect(
            lambda: self.handle_ready_read(client_socket))

    def handle_disconnection(self, client_socket):
        print("Client Disconnected")
        client_socket.deleteLater()

    def handle_ready_read(self, client_socket):
        data = client_socket.readAll().data().decode("utf-8")
        print(f"Data Received: {data}")

        temp_path = os.path.join(QStandardPaths.writableLocation(
            QStandardPaths.StandardLocation.TempLocation), "cw_script.py")
        with open(temp_path, "w", encoding="utf-8") as file:
            file.write(data)

        if os.path.exists(temp_path):
            uc.run_external_program_from_custom_directory(temp_path)

        os.remove(temp_path)


if __name__ == '__main__':
    server_handler = ServerHandler()
    server_handler.run_event_loop()
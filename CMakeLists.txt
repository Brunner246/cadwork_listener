cmake_minimum_required(VERSION 3.30)
project(cadwork_listener)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(QT_VERSION 5)
set(CADLIB_DIR "C:\\source\\cadlib")
set(CADWORK_VERSION 30)

add_subdirectory(src)

target_compile_definitions(${PROJECT_NAME} PRIVATE CWAPI3D_PLUGIN_NAME=L"VS Code Listener")
target_compile_definitions(${PROJECT_NAME} PRIVATE CWAPI3D_AUTHOR_NAME=L"Michael Brunner")
target_compile_definitions(${PROJECT_NAME} PRIVATE CWAPI3D_AUTHOR_EMAIL=L"brunner@cadwork.swiss")

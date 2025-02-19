#cmake_minimum_required(VERSION 3.30)
#project(cadwork_listener)
#
#set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTORCC ON)
#set(CMAKE_AUTOUIC ON)
#set(QT_VERSION 5)
#set(CADLIB_DIR "C:\\source\\cadlib")
#set(CADWORK_VERSION 30)

find_package(CwAPI3D CONFIG REQUIRED)

set(CMAKE_PREFIX_PATH "${CADLIB_DIR}\\3rdParty\\v_${CADWORK_VERSION}.0\\Qt\\x64\\lib\\cmake\\Qt${QT_VERSION}")

find_package(Qt${QT_VERSION} COMPONENTS
        Core
        Network
        Widgets
        REQUIRED)

add_library(cadwork_listener SHARED library.cpp
        ServerHandler.cpp
        ServerHandler.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION}::Core
        Qt${QT_VERSION}::Network
        Qt${QT_VERSION}::Widgets
        CwAPI3D::CwAPI3D
)


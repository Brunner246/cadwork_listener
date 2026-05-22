find_package(CwAPI3D CONFIG REQUIRED)

find_package(Qt${QT_VERSION} COMPONENTS
        Core
        Network
        Widgets
        REQUIRED)

add_library(cadwork_listener SHARED library.cpp
        ServerHandler.cpp
        ServerHandler.h
        ClientSession.cpp
        ClientSession.h
        ScriptExecutor.cpp
        ScriptExecutor.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION}::Core
        Qt${QT_VERSION}::Network
        Qt${QT_VERSION}::Widgets
        CwAPI3D::CwAPI3D
)


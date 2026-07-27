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
        ProtocolCodec.cpp
        ProtocolCodec.h
        ScriptQueue.cpp
        ScriptQueue.h
        NullEventSink.cpp
        NullEventSink.h
        SocketEventSink.cpp
        SocketEventSink.h
        NoopOutputCapture.h
        ports/RunEvent.h
        ports/RunEventSink.h
        ports/ScriptRunner.h
        ports/OutputCapture.h
)

target_include_directories(cadwork_listener PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION}::Core
        Qt${QT_VERSION}::Network
        Qt${QT_VERSION}::Widgets
        CwAPI3D::CwAPI3D
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()



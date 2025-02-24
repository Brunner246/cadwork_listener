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


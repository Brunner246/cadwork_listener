cmake_minimum_required(VERSION 3.30)
project(cadwork_listener)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(QT_VERSION 6)

enable_testing()

# CwAPI3D: prefer an installed package (e.g. vcpkg); otherwise fetch the
# header-only SDK from GitHub via FetchContent.
find_package(CwAPI3D CONFIG QUIET)
if(NOT TARGET CwAPI3D::CwAPI3D)
    include(FetchContent)
    set(CWAPI3D_GIT_REPOSITORY "https://github.com/cwapi3d/cwapi3dcpp.git"
        CACHE STRING "CwAPI3D Git repository URL")
    set(CWAPI3D_GIT_TAG "main"
        CACHE STRING "CwAPI3D Git tag / branch / commit")
    message(STATUS "CwAPI3D not found via find_package; fetching ${CWAPI3D_GIT_REPOSITORY} @ ${CWAPI3D_GIT_TAG}")
    FetchContent_Declare(
        CwAPI3D
        GIT_REPOSITORY ${CWAPI3D_GIT_REPOSITORY}
        GIT_TAG        ${CWAPI3D_GIT_TAG}
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(CwAPI3D)
else()
    message(STATUS "Using installed CwAPI3D package")
endif()

add_subdirectory(src)
add_subdirectory(tests)

target_compile_definitions(${PROJECT_NAME} PRIVATE CWAPI3D_PLUGIN_NAME=L"VS Code Listener")
target_compile_definitions(${PROJECT_NAME} PRIVATE CWAPI3D_AUTHOR_NAME=L"Michael Brunner")
target_compile_definitions(${PROJECT_NAME} PRIVATE CWAPI3D_AUTHOR_EMAIL=L"brunner@cadwork.swiss")

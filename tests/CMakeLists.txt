# Headless unit-test harness (no cadwork / plugin DLL).
# Seeds 02+ add ProtocolCodec / ScriptQueue cases against this target.

find_package(Qt${QT_VERSION} COMPONENTS Core Test REQUIRED)

add_executable(listener_unit_tests
        fixture_smoke_test.cpp
)

target_link_libraries(listener_unit_tests PRIVATE
        Qt${QT_VERSION}::Core
        Qt${QT_VERSION}::Test
)

# Source-tree fixtures so goldens stay editable without a copy step.
# Normalize separators for the compile definition string.
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fixtures" LISTENER_FIXTURES_DIR)
target_compile_definitions(listener_unit_tests PRIVATE
        FIXTURES_DIR="${LISTENER_FIXTURES_DIR}"
)

set_target_properties(listener_unit_tests PROPERTIES
        AUTOMOC ON
)

add_test(NAME listener_unit_tests COMMAND listener_unit_tests)

# ctest launches the exe without the developer shell PATH; prepend Qt runtime DLLs.
if(WIN32)
    get_filename_component(_listener_qt_prefix "${Qt${QT_VERSION}_DIR}/../../.." ABSOLUTE)
    set(_listener_qt_bin "${_listener_qt_prefix}/bin")
    if(EXISTS "${_listener_qt_bin}")
        set_tests_properties(listener_unit_tests PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_listener_qt_bin}"
        )
    endif()
endif()

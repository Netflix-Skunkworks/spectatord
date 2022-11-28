cmake_minimum_required(VERSION 3.13)

project(spectatord)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-fno-omit-frame-pointer "$<$<CONFIG:Debug>:-fsanitize=address>")
add_link_options(-fno-omit-frame-pointer "$<$<CONFIG:Debug>:-fsanitize=address>")

include(CTest)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_subdirectory(admin)
add_subdirectory(bench)
add_subdirectory(server)
add_subdirectory(spectator)
add_subdirectory(tools)
add_subdirectory(util)

#-- spectator_test test executable
file(GLOB spectator_test_source_files
    "bin/test_main.cc"
    "spectator/*test.cc"
    "spectator/http_server.*"
    "spectator/test_utils.*"
)
add_executable(spectator_test ${spectator_test_source_files})
target_include_directories(spectator_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(spectator_test
    sample_cfg
    spectator
    util
    ${CONAN_LIBS}
)
add_test(
    NAME spectator_test
    COMMAND spectator_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#-- spectatord_test test executable
add_executable(spectatord_test
    "admin/admin_server_test.cc"
    "bin/test_main.cc"
    "server/proc_utils_test.cc"
    "server/spectatord_test.cc"
    "spectator/test_utils.cc"
    "spectator/test_utils.h"
)
target_include_directories(spectatord_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(spectatord_test admin_server spectatord ${CONAN_LIBS})
add_test(
    NAME spectatord_test
    COMMAND spectatord_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#-- spectatord_main executable
option(NFLX_INTERNAL "Netflix internal build" OFF)
add_executable(spectatord_main "bin/spectatord_main.cc")
target_include_directories(spectatord_main PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/server
)
if(NFLX_INTERNAL)
    message(STATUS "Using Netflix config")
    target_link_libraries(spectatord_main
        netflix_cfg
        spectatord
        admin_server
        ${CONAN_LIBS}
    )
else()
    message(STATUS "Using sample config")
    target_link_libraries(spectatord_main
        sample_cfg
        spectatord
        admin_server
        ${CONAN_LIBS}
    )
endif()
target_link_options(spectatord_main PRIVATE
    "$<$<CONFIG:Release>:-static-libstdc++>"
)

#-- metrics_gen executable
add_executable(metrics_gen
    "server/local.h"
    "tools/metrics_gen.cc"
)
target_include_directories(metrics_gen PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/server
)
target_link_libraries(metrics_gen ${CONAN_LIBS})
target_link_options(metrics_gen PRIVATE -pthread)

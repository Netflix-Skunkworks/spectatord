cmake_minimum_required(VERSION 3.23)

project(spectatord)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(-pedantic -Werror -Wall -Wno-missing-braces -fno-omit-frame-pointer "$<$<CONFIG:Debug>:-fsanitize=address>")

include(CTest)

find_package(absl REQUIRED)
find_package(asio REQUIRED)
find_package(Backward REQUIRED)
find_package(benchmark REQUIRED)
find_package(fmt REQUIRED)
find_package(GTest REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Poco REQUIRED)
find_package(protobuf REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(spdlog REQUIRED)
find_package(tsl-hopscotch-map REQUIRED)
find_package(xxHash REQUIRED)
find_package(ZLIB REQUIRED)


add_subdirectory(admin)
add_subdirectory(bench)
add_subdirectory(metatron)
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
target_link_libraries(spectator_test
    sample_cfg
    spectator
    util
    Backward::Backward
    gtest::gtest
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
target_link_libraries(spectatord_test
    admin_server
    spectatord
    Backward::Backward
    gtest::gtest
)
add_test(
    NAME spectatord_test
    COMMAND spectatord_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#-- spectatord_main executable
option(NFLX_INTERNAL "Netflix internal build" OFF)
add_executable(spectatord_main "bin/spectatord_main.cc")
if(NFLX_INTERNAL)
    message(STATUS "Using Netflix config")
    target_link_libraries(spectatord_main
        netflix_cfg
        admin_server
        spectatord
        Backward::Backward
    )
else()
    message(STATUS "Using sample config")
    target_link_libraries(spectatord_main
        sample_cfg
        admin_server
        spectatord
        Backward::Backward
    )
endif()
# required to allow running on older systems, such as bionic
target_link_options(spectatord_main PRIVATE "-static-libstdc++")

#-- metrics_gen executable
add_executable(metrics_gen
    "server/local.h"
    "tools/metrics_gen.cc"
)
target_link_libraries(metrics_gen
    abseil::abseil
    asio::asio
    fmt::fmt
    spdlog::spdlog
)
target_link_options(metrics_gen PRIVATE -pthread)

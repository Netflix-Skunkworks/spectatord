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

add_subdirectory(lib/admin)
add_subdirectory(lib/bench)
add_subdirectory(lib/metatron)
add_subdirectory(lib/server)
add_subdirectory(lib/spectator)
add_subdirectory(lib/tools)
add_subdirectory(lib/util)

#-- spectator_test test executable
add_executable(spectator_test
    "lib/bin/test_main.cc"
    "lib/spectator/encoding/compressed_buffer_test.cc"
    "lib/spectator/encoding/gzip_test.cc"
    "lib/spectator/encoding/smile_test.cc"
    "lib/spectator/http/http_client_test.cc"
    "lib/spectator/http/http_server.cc"
    "lib/spectator/meters/age_gauge_test.cc"
    "lib/spectator/meters/config_test.cc"
    "lib/spectator/meters/counter_test.cc"
    "lib/spectator/meters/dist_summary_test.cc"
    "lib/spectator/meters/gauge_test.cc"
    "lib/spectator/meters/id_test.cc"
    "lib/spectator/meters/max_gauge_test.cc"
    "lib/spectator/meters/monotonic_counter_test.cc"
    "lib/spectator/meters/monotonic_counter_uint_test.cc"
    "lib/spectator/meters/monotonic_sampled_test.cc"
    "lib/spectator/meters/perc_buckets_test.cc"
    "lib/spectator/meters/tags_test.cc"
    "lib/spectator/meters/timer_test.cc"
    "lib/spectator/registry/distinct_count_sketch_test.cc"
    "lib/spectator/registry/perc_dist_summary_test.cc"
    "lib/spectator/registry/perc_timer_test.cc"
    "lib/spectator/registry/registry_test.cc"
    "lib/spectator/registry/test_utils.cc"
    "lib/spectator/strings/string_intern_test.cc"
    "lib/spectator/strings/strings_test.cc"
)
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
    "lib/admin/admin_server_test.cc"
    "lib/bin/test_main.cc"
    "lib/server/proc/proc_utils_test.cc"
    "lib/server/spectatord_test.cc"
    "lib/spectator/registry/test_utils.cc"
    "lib/spectator/registry/test_utils.h"
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

#-- util_test test executable
add_executable(util_test
    "lib/bin/test_main.cc"
    "lib/util/logging/unix_socket_log_sink_test.cc"
)
target_link_libraries(util_test
    util
    Backward::Backward
    gtest::gtest
)
add_test(
    NAME util_test
    COMMAND util_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#-- spectatord_main executable
option(NFLX_INTERNAL "Netflix internal build" OFF)
add_executable(spectatord_main "lib/bin/spectatord_main.cc")
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
    "lib/server/transport/local.h"
    "lib/tools/metrics_gen.cc"
)
target_include_directories(metrics_gen PRIVATE ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(metrics_gen
    abseil::abseil
    asio::asio
    fmt::fmt
    spdlog::spdlog
)
target_link_options(metrics_gen PRIVATE -pthread)

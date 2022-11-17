#-- fmt_bench test executable
add_executable(fmt_bench "measurements_fmt_bench.cc")
target_include_directories(fmt_bench PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/server
)
target_link_libraries(fmt_bench spectatord ${CONAN_LIBS})

#-- parse_bench test executable
add_executable(parse_bench "parse_bench.cc")
target_include_directories(parse_bench PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/server
)
target_link_libraries(parse_bench spectatord ${CONAN_LIBS})

#-- ms_bench test executable
add_executable(ms_bench "get_measurement_bench.cc")
target_include_directories(ms_bench PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/server
)
target_link_libraries(ms_bench spectatord ${CONAN_LIBS})

#-- statsd_bench test executable
add_executable(statsd_bench "statsd_bench.cc")
target_include_directories(statsd_bench PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/server
)
target_link_libraries(statsd_bench spectatord ${CONAN_LIBS})

#-- tags_bench test executable
add_executable(tags_bench "bench_tags.cc")
target_include_directories(tags_bench PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tags_bench spectator ${CONAN_LIBS})

#-- to_valid_chars test executable
add_executable(to_valid_chars "to_valid_bench.cc")
target_link_libraries(to_valid_chars ${CONAN_LIBS})

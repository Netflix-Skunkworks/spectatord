#-- fmt_bench test executable
add_executable(fmt_bench "measurements_fmt_bench.cc")
target_link_libraries(fmt_bench
    spectatord
    benchmark::benchmark_main
)

#-- parse_bench test executable
add_executable(parse_bench "parse_bench.cc")
target_link_libraries(parse_bench
    spectatord
    benchmark::benchmark_main
)

#-- ms_bench test executable
add_executable(ms_bench "get_measurement_bench.cc")
target_link_libraries(ms_bench
    spectatord
    benchmark::benchmark_main
)

#-- statsd_bench test executable
add_executable(statsd_bench "statsd_bench.cc")
target_link_libraries(statsd_bench
    spectatord
    benchmark::benchmark_main
)

#-- tags_bench test executable
add_executable(tags_bench "bench_tags.cc")
target_link_libraries(tags_bench
    spectator
    benchmark::benchmark_main
)

#-- to_valid_chars test executable
add_executable(to_valid_chars "to_valid_bench.cc")
target_link_libraries(to_valid_chars
    benchmark::benchmark_main
)

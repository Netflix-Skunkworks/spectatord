add_executable(fmt_bench "measurements_fmt_bench.cc")
target_link_libraries(fmt_bench PRIVATE spectatord benchmark::benchmark_main)

add_executable(get_measurement_bench "get_measurement_bench.cc")
target_link_libraries(get_measurement_bench PRIVATE spectatord benchmark::benchmark_main)

add_executable(parse_bench "parse_bench.cc")
target_link_libraries(parse_bench PRIVATE spectatord benchmark::benchmark_main)

add_executable(statsd_bench "statsd_bench.cc")
target_link_libraries(statsd_bench PRIVATE spectatord benchmark::benchmark_main)

add_executable(tags_bench "bench_tags.cc")
target_link_libraries(tags_bench PRIVATE spectator benchmark::benchmark_main)

add_executable(to_valid_bench "to_valid_bench.cc")
target_link_libraries(to_valid_bench PRIVATE benchmark::benchmark_main)

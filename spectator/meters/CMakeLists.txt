file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/spectator/meters")

add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/spectator/meters/percentile_bucket_tags.inc"
    COMMAND "${CMAKE_BINARY_DIR}/bin/gen_perc_bucket_tags" > "${CMAKE_BINARY_DIR}/spectator/meters/percentile_bucket_tags.inc"
    DEPENDS gen_perc_bucket_tags
)
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/spectator/meters/percentile_bucket_values.inc"
    COMMAND "${CMAKE_BINARY_DIR}/bin/gen_perc_bucket_values" > "${CMAKE_BINARY_DIR}/spectator/meters/percentile_bucket_values.inc"
    DEPENDS gen_perc_bucket_values
)
add_custom_target(gen_perc_bucket_incs DEPENDS
    "${CMAKE_BINARY_DIR}/spectator/meters/percentile_bucket_tags.inc"
    "${CMAKE_BINARY_DIR}/spectator/meters/percentile_bucket_values.inc"
)

#-- INTERFACE library for header-only types: id, tags, measurement, meter, config, etc.
add_library(spectator_meter_types INTERFACE)
target_include_directories(spectator_meter_types
    INTERFACE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}/spectator/meters
)
target_link_libraries(spectator_meter_types
    INTERFACE
        spectator_strings
        abseil::abseil
        fmt::fmt
        spdlog::spdlog
        tsl::hopscotch_map
)

#-- STATIC library for concrete meter implementations
add_library(spectator_meters STATIC
    "counter.cc"
    "dist_summary.cc"
    "gauge.cc"
    "max_gauge.cc"
    "monotonic_counter.cc"
    "monotonic_counter_uint.cc"
    "monotonic_sampled.cc"
    "percentile_buckets.cc"
    "timer.cc"
)
target_link_libraries(spectator_meters
    PUBLIC
        spectator_meter_types
)
add_dependencies(spectator_meters gen_perc_bucket_incs)

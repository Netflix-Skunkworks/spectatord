#-- sample config library
add_library(sample_cfg OBJECT "sample_config.cc")
target_link_libraries(sample_cfg spectator)

#-- netflix config library
if(NFLX_INTERNAL)
    add_library(netflix_cfg OBJECT "netflix_config.cc")
    target_link_libraries(netflix_cfg spectator)
endif()

# -- spectator library
add_library(spectator OBJECT
    "age_gauge.h"
    "atomicnumber.h"
    "common_refs.cc"
    "common_refs.h"
    "compressed_buffer.cc"
    "compressed_buffer.h"
    "config.h"
    "counter.cc"
    "counter.h"
    "detail/perc_policy.h"
    "dist_stats.h"
    "dist_summary.cc"
    "dist_summary.h"
    "gauge.cc"
    "gauge.h"
    "gzip.cc"
    "gzip.h"
    "http_client.cc"
    "http_client.h"
    "id.h"
    "log_entry.h"
    "max_gauge.cc"
    "max_gauge.h"
    "measurement.h"
    "meter.h"
    "monotonic_counter.cc"
    "monotonic_counter.h"
    "monotonic_sampled.cc"
    "monotonic_sampled.h"
    "percentile_bucket_tags.inc"
    "percentile_bucket_values.inc"
    "percentile_buckets.cc"
    "percentile_buckets.h"
    "percentile_distribution_summary.h"
    "percentile_timer.h"
    "publisher.h"
    "registry.cc"
    "registry.h"
    "smile.cc"
    "smile.h"
    "string_intern.h"
    "string_pool.cc"
    "string_pool.h"
    "strings.cc"
    "strings.h"
    "tags.h"
    "timer.cc"
    "timer.h"
    "util.h"
    "valid_chars.inc"
    "version.h"
)
target_include_directories(spectator PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(spectator util ${CONAN_LIBS})

#-- file generators, must exist where the outputs are referenced
add_custom_command(
    OUTPUT "percentile_bucket_tags.inc"
    COMMAND "${CMAKE_BINARY_DIR}/bin/gen_perc_bucket_tags" > "${CMAKE_SOURCE_DIR}/spectator/percentile_bucket_tags.inc"
    DEPENDS gen_perc_bucket_tags
)

add_custom_command(
    OUTPUT "percentile_bucket_values.inc"
    COMMAND "${CMAKE_BINARY_DIR}/bin/gen_perc_bucket_values" > "${CMAKE_SOURCE_DIR}/spectator/percentile_bucket_values.inc"
    DEPENDS gen_perc_bucket_values
)

add_custom_command(
    OUTPUT "valid_chars.inc"
    COMMAND "${CMAKE_BINARY_DIR}/bin/gen_valid_chars" > "${CMAKE_SOURCE_DIR}/spectator/valid_chars.inc"
    DEPENDS gen_valid_chars
)

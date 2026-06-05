file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/spectator/strings")

add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/spectator/strings/valid_chars.inc"
    COMMAND "${CMAKE_BINARY_DIR}/bin/gen_valid_chars" > "${CMAKE_BINARY_DIR}/spectator/strings/valid_chars.inc"
    DEPENDS gen_valid_chars
)
add_custom_target(gen_valid_chars_inc
    DEPENDS "${CMAKE_BINARY_DIR}/spectator/strings/valid_chars.inc"
)

add_library(spectator_strings STATIC
    "common_refs.cc"
    "string_pool.cc"
    "strings.cc"
)
target_include_directories(spectator_strings
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}/spectator/strings
)
target_link_libraries(spectator_strings
    PUBLIC
        abseil::abseil
        fmt::fmt
        tsl::hopscotch_map
        xxHash::xxhash
)
add_dependencies(spectator_strings gen_valid_chars_inc)

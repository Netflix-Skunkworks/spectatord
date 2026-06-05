add_subdirectory(strings)
add_subdirectory(encoding)
add_subdirectory(meters)
add_subdirectory(http)
add_subdirectory(registry)

#-- Umbrella target: existing callers can continue to link against spectator
add_library(spectator INTERFACE)
target_link_libraries(spectator
    INTERFACE
        spectator_registry
        spectator_meters
        spectator_http
        spectator_encoding
        spectator_strings
)

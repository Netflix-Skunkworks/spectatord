#-- INTERFACE library: provides http_client.h include path and transitive link deps.
#-- http_client.cc is compiled as part of spectator_registry (it depends on registry.h
#-- via log_entry.h, so it needs spectator_registry's full dep set at compile time).
add_library(spectator_http INTERFACE)
target_include_directories(spectator_http INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(spectator_http
    INTERFACE
        spectator_encoding
        spectator_strings
        abseil::abseil
)

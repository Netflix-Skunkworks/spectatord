add_library(spectator_http STATIC
    "http_client.cc"
)
target_include_directories(spectator_http
    PUBLIC
        ${CMAKE_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/spectator/http
)
target_link_libraries(spectator_http
    PUBLIC
        spectator_encoding
        spectator_strings
        abseil::abseil
    PRIVATE
        metatron
        CURL::libcurl
)

add_library(spectator_encoding STATIC
    "gzip.cc"
    "compressed_buffer.cc"
    "smile.cc"
)
target_include_directories(spectator_encoding
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)
target_link_libraries(spectator_encoding
    PUBLIC
        ZLIB::ZLIB
        fmt::fmt
)

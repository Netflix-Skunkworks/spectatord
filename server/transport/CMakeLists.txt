add_library(spectatord_transport STATIC
    "local_server.cc"
    "udp_server.cc"
)
target_include_directories(spectatord_transport PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(spectatord_transport
    PUBLIC
        asio::asio
    PRIVATE
        spectatord_proc
        util
)

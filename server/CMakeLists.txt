add_subdirectory(proc)
add_subdirectory(transport)

add_library(spectatord STATIC "spectatord.cc")
target_include_directories(spectatord PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(spectatord
    PUBLIC
        spectator
    PRIVATE
        spectatord_proc
        spectatord_transport
        util
        abseil::abseil
        asio::asio
)

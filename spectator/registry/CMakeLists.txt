add_library(spectator_registry STATIC
    "registry.cc"
    "${CMAKE_SOURCE_DIR}/spectator/http/http_client.cc"
)
target_include_directories(spectator_registry
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)
target_link_libraries(spectator_registry
    PUBLIC
        spectator_meters
        spectator_http
        metatron
        rapidjson
    PRIVATE
        util
        CURL::libcurl
        asio::asio
        spdlog::spdlog
        tsl::hopscotch_map
)

#-- sample config library
add_library(sample_cfg STATIC "sample_config.cc")
target_include_directories(sample_cfg PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(sample_cfg PUBLIC spectator_registry)

#-- netflix config library (internal builds only)
if(NFLX_INTERNAL)
    add_library(netflix_cfg STATIC "netflix_config.cc")
    target_include_directories(netflix_cfg PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(netflix_cfg PUBLIC spectator_registry)
endif()

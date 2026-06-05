add_library(util_logging STATIC "logger.cc")
target_include_directories(util_logging PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(util_logging
    PUBLIC  spdlog::spdlog fmt::fmt asio::asio
    PRIVATE rapidjson
)

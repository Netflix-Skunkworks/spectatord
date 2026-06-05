#-- util library
add_library(util STATIC
    "files.h"
    "insight_log_sinks.h"
    "json_log_formatter.h"
    "logger.cc"
    "logger.h"
    "unix_socket_log_sink.h"
)
target_include_directories(util PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(util
    PUBLIC  spdlog::spdlog fmt::fmt asio::asio
    PRIVATE rapidjson
)

#-- util library
add_library(util OBJECT
    "files.h"
    "insight_log_sinks.h"
    "json_log_formatter.h"
    "logger.cc"
    "logger.h"
    "unix_socket_log_sink.h"
)
target_link_libraries(util spdlog::spdlog fmt::fmt rapidjson asio::asio)

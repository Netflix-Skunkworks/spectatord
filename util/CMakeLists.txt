#-- util library
add_library(util OBJECT
    "files.h"
    "logger.cc"
    "logger.h"
    "tcp_log_sink.h"
)
target_link_libraries(util spdlog::spdlog fmt::fmt rapidjson)

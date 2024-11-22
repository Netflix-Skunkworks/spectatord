#-- util library
add_library(util OBJECT
    "files.h"
    "logger.cc"
    "logger.h"
)
target_link_libraries(util spdlog::spdlog)

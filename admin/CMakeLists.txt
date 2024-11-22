#-- admin server library
add_library(admin_server
    "admin_server.cc"
    "admin_server.h"
)
target_link_libraries(admin_server
    spectator
    Poco::Poco
)

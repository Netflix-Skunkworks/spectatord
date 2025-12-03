#-- spectatord library
add_library(spectatord
    "expiring_cache.h"
    "handler.h"
    "local.h"
    "local_server.cc"
    "local_server.h"
    "proc_utils.cc"
    "proc_utils.h"
    "spectatord.cc"
    "spectatord.h"
    "udp_server.cc"
    "udp_server.h"
)
target_link_libraries(spectatord
    spectator
    util
    abseil::abseil
    asio::asio
)


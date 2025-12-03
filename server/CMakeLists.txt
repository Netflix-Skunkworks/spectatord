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

# Link against libsystemd if available
if(LIBSYSTEMD_FOUND)
    target_link_libraries(spectatord ${LIBSYSTEMD_LIBRARIES})
    target_include_directories(spectatord PRIVATE ${LIBSYSTEMD_INCLUDE_DIRS})
endif()

#-- admin server library
add_library(admin_server
    "admin_server.cc"
    "admin_server.h"
)
target_include_directories(admin_server PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(admin_server ${CONAN_LIBS})

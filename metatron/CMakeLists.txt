#-- metatron verifier library
if(NFLX_INTERNAL)
    add_library(metatron
        "auth_context.pb.cc"
        "auth_context.pb.h"
        "metatron_config.cc"
        "metatron_config.h"
    )
else()
    add_library(metatron
        "metatron_config.h"
        "metatron_sample.cc"
    )
endif()
target_link_libraries(metatron util
    CURL::libcurl
    openssl::openssl
    protobuf::protobuf
)

#-- protobuf generated files must exist in both the SOURCE_DIR and the BINARY_DIR
add_custom_command(
    OUTPUT auth_context.pb.cc auth_context.pb.h
    COMMAND protoc --proto_path=${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_SOURCE_DIR} auth_context.proto
    COMMAND protoc --proto_path=${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_BINARY_DIR} auth_context.proto
)

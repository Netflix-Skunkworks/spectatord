add_subdirectory(logging)

#-- Umbrella target: existing callers can continue to link against util
add_library(util INTERFACE)
target_include_directories(util INTERFACE ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(util INTERFACE util_logging)

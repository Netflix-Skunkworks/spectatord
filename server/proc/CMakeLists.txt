add_library(spectatord_proc STATIC "proc_utils.cc")
target_include_directories(spectatord_proc PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(spectatord_proc PRIVATE util abseil::abseil)

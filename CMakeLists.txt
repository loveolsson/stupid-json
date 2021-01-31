add_library(stupid-json STATIC)

add_subdirectory(external/fast_float fast_float)
target_link_libraries(stupid-json PUBLIC fast_float)

target_include_directories(stupid-json PUBLIC include/)
target_sources(stupid-json PRIVATE src/arena.cpp)
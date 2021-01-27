add_library(stupid-json STATIC)
target_include_directories(stupid-json PUBLIC include/)
target_sources(stupid-json PRIVATE src/arena.cpp)
cmake_minimum_required(VERSION 3.9)
project(stupid-json-tests)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(googletest gtest EXCLUDE_FROM_ALL)
add_subdirectory(../ stupid-json)

include(GoogleTest)
add_executable(stupid-json-tests src/main.cpp)
target_link_libraries(stupid-json-tests PRIVATE gtest)
target_link_libraries(stupid-json-tests PRIVATE gmock)
target_link_libraries(stupid-json-tests PRIVATE stupid-json)

#include(CheckIPOSupported)
#check_ipo_supported(RESULT iporesult)
#if(iporesult)
#  set_property(TARGET stupid-json-tests PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
#endif()

enable_testing()
gtest_add_tests(stupid-json-tests "${CMAKE_CURRENT_SOURCE_DIR}" AUTO)
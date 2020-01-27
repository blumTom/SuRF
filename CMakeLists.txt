cmake_minimum_required (VERSION 2.6)

add_subdirectory(tlx)

project (SuRF)

message(STATUS "Configuring..." ${CMAKE_PROJECT_NAME})

if (APPLE)
  link_directories(/usr/local/lib/)
endif (APPLE)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -Wall -Wno-unused-variable -Wno-unused-value -Wno-unused-function -mpopcnt -pthread -std=c++17")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -Wall -Werror -Wno-unused-variable -Wno-unused-value -Wno-overflow -Wno-sign-compare -Wno-unused-function -mpopcnt -pthread -std=c++17")

option(COVERALLS "Generate coveralls data" OFF)

if (COVERALLS)
  include("${CMAKE_CURRENT_SOURCE_DIR}/CodeCoverage.cmake")
  append_coverage_compiler_flags()
  set(COVERAGE_EXCLUDES 'ARF/*' 'bench/*' 'test/*' '/usr/*' '/lib/*')
  setup_target_for_coverage(
    NAME coverage
    EXECUTABLE make test
    )
else()
  add_definitions(-DNDEBUG)
endif()

enable_testing()

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tlx
        ${CMAKE_CURRENT_SOURCE_DIR}/perfevent
        ${GTEST_INCLUDE_DIR}
        ${GMOCK_INCLUDE_DIR}
        ${GFLAGS_INCLUDE_DIR})

file(COPY test/words.txt DESTINATION ${CMAKE_BINARY_DIR})

add_executable(simple_example simple_example.cpp)
target_link_libraries(simple_example surf)

add_executable(simple_bench simple_bench.cpp)
target_link_libraries(simple_bench surf tlx)

if (NOT APPLE)
    add_executable(perf_bench perf_bench.cpp)
    target_link_libraries(perf_bench surf tlx)
endif()

include("${CMAKE_SOURCE_DIR}/src/local.cmake")
include("${CMAKE_SOURCE_DIR}/test/unitTest/local.cmake")
include("${CMAKE_SOURCE_DIR}/bench/local.cmake")

#include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ARF/include")
#add_subdirectory(ARF)

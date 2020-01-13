# ---------------------------------------------------------------------------
# SURF
# ---------------------------------------------------------------------------

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIR})

# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

file(GLOB_RECURSE TEST_CC test/unitTest/*.cpp)
list(REMOVE_ITEM TEST_CC test/unitTest/tester.cpp)

# ---------------------------------------------------------------------------
# Tester
# ---------------------------------------------------------------------------

add_executable(tester test/unitTest/tester.cpp ${TEST_CC})
target_link_libraries(tester surf ${GTEST_LIBRARIES})
#enable_testing()
#add_test(surf tester)
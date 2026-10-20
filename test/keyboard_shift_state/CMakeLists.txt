add_executable(KeyboardShiftStateTest
  KeyboardShiftStateTest.cpp
)

target_include_directories(KeyboardShiftStateTest PRIVATE
  ${REPO_ROOT}/src/activities/util
)

target_link_libraries(KeyboardShiftStateTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(KeyboardShiftStateTest)

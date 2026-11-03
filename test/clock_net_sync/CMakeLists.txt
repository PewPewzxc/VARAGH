add_executable(ClockNetSyncTest
  ClockNetSyncTest.cpp
)
target_link_libraries(ClockNetSyncTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(ClockNetSyncTest)

add_executable(CalendarMathTest
  CalendarMathTest.cpp
)
target_link_libraries(CalendarMathTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(CalendarMathTest)

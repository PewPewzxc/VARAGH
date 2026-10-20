add_executable(SeriesPositionTest
  SeriesPositionTest.cpp
)

target_include_directories(SeriesPositionTest PRIVATE
  ${REPO_ROOT}/src/util
)

target_link_libraries(SeriesPositionTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(SeriesPositionTest)

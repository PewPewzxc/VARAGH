add_executable(WordSelectStepTest
  WordSelectStepTest.cpp
)

target_include_directories(WordSelectStepTest PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/EpdFont
)

target_link_libraries(WordSelectStepTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WordSelectStepTest)

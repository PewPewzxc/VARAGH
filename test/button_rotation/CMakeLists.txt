add_executable(ButtonRotationTest
  ButtonRotationTest.cpp
)

target_include_directories(ButtonRotationTest PRIVATE
  ${REPO_ROOT}/src/util
)

target_link_libraries(ButtonRotationTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ButtonRotationTest)

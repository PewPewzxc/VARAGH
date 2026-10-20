add_executable(GoGameTest
  GoGameTest.cpp
  ${REPO_ROOT}/src/games/GoGame.cpp
)
target_link_libraries(GoGameTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(GoGameTest)

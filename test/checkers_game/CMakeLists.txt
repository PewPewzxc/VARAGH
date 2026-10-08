add_executable(CheckersGameTest
  CheckersGameTest.cpp
  ${REPO_ROOT}/src/games/CheckersGame.cpp
)
target_link_libraries(CheckersGameTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(CheckersGameTest)

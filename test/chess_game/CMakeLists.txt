add_executable(ChessGameTest
  ChessGameTest.cpp
  ${REPO_ROOT}/src/games/ChessGame.cpp
)
target_link_libraries(ChessGameTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(ChessGameTest)

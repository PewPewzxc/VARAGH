add_executable(SudokuGameTest
  SudokuGameTest.cpp
  ${REPO_ROOT}/src/games/SudokuGame.cpp
)
target_link_libraries(SudokuGameTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(SudokuGameTest)

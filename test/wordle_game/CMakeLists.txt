add_executable(WordleGameTest
  WordleGameTest.cpp
  ${REPO_ROOT}/src/games/WordleGame.cpp
)
target_link_libraries(WordleGameTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(WordleGameTest)

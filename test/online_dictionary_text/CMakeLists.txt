add_executable(OnlineDictionaryTextTest
  OnlineDictionaryTextTest.cpp
  ${REPO_ROOT}/src/util/OnlineDictionaryText.cpp
)
target_link_libraries(OnlineDictionaryTextTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(OnlineDictionaryTextTest)

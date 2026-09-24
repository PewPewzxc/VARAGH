add_executable(GermanStemmerTest
  GermanStemmerTest.cpp
  ${REPO_ROOT}/src/util/GermanStemmer.cpp
)

target_include_directories(GermanStemmerTest PRIVATE
  ${REPO_ROOT}/src/util
)

target_link_libraries(GermanStemmerTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(GermanStemmerTest)

add_executable(WordFormsTest
  WordFormsTest.cpp
  ${REPO_ROOT}/src/util/WordForms.cpp
)

target_include_directories(WordFormsTest PRIVATE
  ${REPO_ROOT}/src/util
)

target_link_libraries(WordFormsTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WordFormsTest)

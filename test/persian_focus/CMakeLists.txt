add_executable(PersianFocusTest
  PersianFocusTest.cpp
)

target_include_directories(PersianFocusTest PRIVATE
  ${REPO_ROOT}/lib/Epub/Epub
)

target_link_libraries(PersianFocusTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(PersianFocusTest)

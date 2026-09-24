add_executable(HighlightStoreTest
  HighlightStoreTest.cpp
  ${REPO_ROOT}/src/highlights/HighlightStore.cpp
)
# Stubs first so the store picks up the host file system instead of the SD card.
target_include_directories(HighlightStoreTest BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/highlights
)
target_link_libraries(HighlightStoreTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(HighlightStoreTest)

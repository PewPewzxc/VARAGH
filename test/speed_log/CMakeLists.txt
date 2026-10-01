add_executable(SpeedLogTest
  SpeedLogTest.cpp
  ${REPO_ROOT}/lib/SpeedLog/SpeedLog.cpp
  ${REPO_ROOT}/lib/EpdFont/FontFileMirror.cpp
)

target_include_directories(SpeedLogTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/SpeedLog
  ${REPO_ROOT}/lib/EpdFont
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/lib/MemoryBudget
)

find_package(Threads REQUIRED)
target_link_libraries(SpeedLogTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
  Threads::Threads
)

gtest_discover_tests(SpeedLogTest)

# TEST_SRC_LIST is a semicolon-separated list of items
file(GLOB_RECURSE TEST_SRC_LIST "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

# Evaluate a group of commands for each value in a list.
foreach (TEST_SRC ${TEST_SRC_LIST})
  get_filename_component(TEST_EXE_NAME ${TEST_SRC} NAME_WE)
  add_executable(${TEST_EXE_NAME} ${TEST_SRC})
  target_link_libraries(${TEST_EXE_NAME} PRIVATE rayjoin_lib_vk GTest::gtest_main)
  #  target_include_directories(${TEST_EXE_NAME} PRIVATE ${XBLSM_TEST_INCLUDE_DIR})
endforeach ()

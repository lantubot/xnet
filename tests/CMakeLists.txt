# XNet unit tests (CTest)
#
# Each test is a standalone executable.  Tests use a minimal custom
# test framework (test_helpers.h) — no external test library needed.

# Discover all test_*.cpp files
file(GLOB XNET_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

foreach(TEST_SOURCE ${XNET_TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})

  target_include_directories(${TEST_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_link_libraries(${TEST_NAME} PRIVATE xnet)

  # Register with CTest
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

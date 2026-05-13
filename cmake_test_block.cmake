# ── Tests ────────────────────────────────────────────────────────────
if(XNET_BUILD_TESTS)
    add_executable(xnet_test_buffer tests/test_buffer.cpp)
    target_link_libraries(xnet_test_buffer PRIVATE xnet)
endif()

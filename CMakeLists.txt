cmake_minimum_required(VERSION 3.16)
project(xnet VERSION 0.1.0 LANGUAGES CXX)

# ── C++ Standard ─────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ──────────────────────────────────────────────────────────
option(XNET_BUILD_EXAMPLES "Build examples" ON)
option(XNET_BUILD_TESTS    "Build tests"    OFF)
option(XNET_USE_EXCEPTIONS "Enable exceptions" OFF)
option(XNET_TESTS          "Use RTTI" OFF)

# ── Compile Options ──────────────────────────────────────────────────
if(NOT XNET_USE_EXCEPTIONS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        string(APPEND CMAKE_CXX_FLAGS " -fno-exceptions")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        string(APPEND CMAKE_CXX_FLAGS " /EHs-c-")
    endif()
endif()

if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /GR-")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Wpedantic -Werror")
endif()

# ── Platform Detection ───────────────────────────────────────────────
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(XNET_PLATFORM_LINUX TRUE)
    set(XNET_PLATFORM "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(XNET_PLATFORM_WINDOWS TRUE)
    set(XNET_PLATFORM "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "ESP32" OR
       CMAKE_SYSTEM_NAME STREQUAL "Generic")
    set(XNET_PLATFORM_ESP32 TRUE)
    set(XNET_PLATFORM "esp32")
else()
    message(WARNING "Unknown platform: ${CMAKE_SYSTEM_NAME}, defaulting to linux")
    set(XNET_PLATFORM_LINUX TRUE)
    set(XNET_PLATFORM "linux")
endif()

# ── Sources ──────────────────────────────────────────────────────────
set(XNET_SOURCES
    src/url.cpp
    src/buffer.cpp
    src/http.cpp
    src/request.cpp
    src/socket_common.cpp
)

set(XNET_PUBLIC_HEADERS
    include/xnet/xnet.h
    include/xnet/error.h
    include/xnet/string_view.h
    include/xnet/buffer.h
    include/xnet/url.h
    include/xnet/http.h
    include/xnet/socket.h
    include/xnet/request.h
)

# ── Platform Sources ─────────────────────────────────────────────────
if(XNET_PLATFORM_LINUX)
    list(APPEND XNET_SOURCES ports/linux/socket_linux.cpp)
elseif(XNET_PLATFORM_WINDOWS)
    list(APPEND XNET_SOURCES ports/windows/socket_windows.cpp)
elseif(XNET_PLATFORM_ESP32)
    list(APPEND XNET_SOURCES ports/esp32/socket_esp32.cpp)
endif()

# ── Library ──────────────────────────────────────────────────────────
add_library(xnet STATIC ${XNET_SOURCES} ${XNET_PUBLIC_HEADERS})
target_include_directories(xnet PUBLIC include)

target_compile_definitions(xnet PRIVATE
    XNET_PLATFORM_${XNET_PLATFORM}
    $<$<BOOL:${XNET_USE_EXCEPTIONS}>:XNET_USE_EXCEPTIONS>
)

if(XNET_PLATFORM_LINUX)
    target_link_libraries(xnet PUBLIC pthread)
elseif(XNET_PLATFORM_WINDOWS)
    target_link_libraries(xnet PUBLIC ws2_32)
endif()

# ── Examples ─────────────────────────────────────────────────────────
if(XNET_BUILD_EXAMPLES)
    add_executable(xnet_http_get examples/http_get.cpp)
    target_link_libraries(xnet_http_get PRIVATE xnet)
endif()

# ── Tests (CTest) ────────────────────────────────────────────────────
if(XNET_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# XNet Build Guide

## 📋 Requirements

| Tool | Minimum Version |
|:-----|:---------------:|
| CMake | 3.16 |
| C++ Compiler | C++20 support (GCC 11+, Clang 14+, MSVC 2022+) |
| clang-format | 14+ (optional, for linting) |

## 🔧 Standard Build

```bash
# Configure
cmake -B build -DXNET_BUILD_TESTS=ON -DXNET_BUILD_EXAMPLES=ON

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

## ⚙️ Build Options

| Option | Default | Description |
|:-------|:-------:|:------------|
| `XNET_BUILD_TESTS` | `ON` | Build unit tests |
| `XNET_BUILD_EXAMPLES` | `ON` | Build examples (`xnet_http_get`) |
| `XNET_BUILD_LINT` | `OFF` | Enable clang-format lint targets |
| `XNET_USE_EXCEPTIONS` | `OFF` | Enable C++ exceptions |
| `XNET_USE_RTTI` | `OFF` | Enable RTTI |

### Local Development (all features)

```bash
cmake -B build \
    -DXNET_BUILD_TESTS=ON \
    -DXNET_BUILD_EXAMPLES=ON \
    -DXNET_BUILD_LINT=ON

cmake --build build
ctest --output-on-failure
cmake --build build --target xnet_lint
```

### Minimal Build (embedded target)

```bash
cmake -B build \
    -DXNET_BUILD_TESTS=OFF \
    -DXNET_BUILD_EXAMPLES=OFF \
    -DXNET_USE_EXCEPTIONS=OFF \
    -DXNET_USE_RTTI=OFF
```

## 🎯 Cross-Compilation

### ESP32 (ESP-IDF)

XNet works with ESP-IDF's CMake-based build system. Add as a component:

**Option A: Component directory**
```bash
mkdir -p components/xnet
cp -r include/ src/ ports/esp32/ CMakeLists.txt components/xnet/
```

Configure the idf_component_register in your component CMakeLists.txt:

```cmake
idf_component_register(
    SRCS "src/url.cc" "src/buffer.cc" "src/http.cc"
         "src/request.cc" "src/socket_common.cc"
         "ports/esp32/socket_esp32.cc"
    INCLUDE_DIRS "include"
)
```

**Option B: External project**
```cmake
# In your main project CMakeLists.txt
add_subdirectory(xnet)
target_link_libraries(${COMPONENT_LIB} INTERFACE xnet)
```

### ARM / Generic Embedded

For any ARM target (Cortex-M, etc.):

```bash
cmake -B build \
    -DCMAKE_SYSTEM_NAME=Generic \
    -DCMAKE_CXX_COMPILER=arm-none-eabi-g++ \
    -DXNET_USE_EXCEPTIONS=OFF \
    -DXNET_USE_RTTI=OFF
```

## 🧪 Running Tests

```bash
# Build and run all tests
cmake -B build -DXNET_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Run a specific test directly
./build/test_buffer
./build/test_str_view
./build/test_url
```

### Test Coverage

| Test File | Tests |
|:----------|:------|
| `test_buffer.cc` | 10 tests: construction, append, pop_front, reserve, resize, swap, find, clear, multiple appends |
| `test_str_view.cc` | 10 tests: construction, comparison, starts_with, find, substr, to_int, empty |
| `test_url.cc` | 9 tests: simple HTTP, HTTPS with port, query/fragment, user/password, relative path, IPv6, empty URL, encode/decode, default ports |

## 🧼 Code Style

XNet follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

```bash
# Check formatting
cmake --build build --target xnet_lint

# Auto-fix formatting
cmake --build build --target xnet_format
```

The `.clang-format` file at the project root defines the exact style rules.

## 🤖 CI/CD

The project uses GitHub Actions with a `cmake-multi-platform.yml` workflow:

- **Triggers:** Push to `master` / `dev`, PRs to `master` / `dev`
- **Matrix:**
  - Ubuntu (GCC, Clang)
  - Windows (MSVC)
- **Checks:**
  - ✅ Compilation with all features enabled
  - ✅ `ctest` — all tests pass
  - ✅ `xnet_lint` — code style conformance

Status badge: [![CI](https://github.com/lantubot/xnet/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/lantubot/xnet/actions/workflows/cmake-multi-platform.yml)

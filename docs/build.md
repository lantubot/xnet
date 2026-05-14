# XNet 构建指南

## 📋 环境要求

| 工具 | 最低版本 |
|:-----|:-------:|
| CMake | 3.16 |
| C++ 编译器 | 支持 C++20（GCC 11+、Clang 14+、MSVC 2022+） |
| clang-format | 14+（可选，用于代码检查） |

## 🔧 标准构建

```bash
# Configure
cmake -B build -DXNET_BUILD_TESTS=ON -DXNET_BUILD_EXAMPLES=ON

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

## ⚙️ 构建选项

| 选项 | 默认值 | 说明 |
|:-------|:-----:|:-----|
| `XNET_BUILD_TESTS` | `ON` | 构建单元测试 |
| `XNET_BUILD_EXAMPLES` | `ON` | 构建示例程序（`xnet_http_get`） |
| `XNET_BUILD_LINT` | `OFF` | 启用 clang-format 代码检查目标 |
| `XNET_USE_EXCEPTIONS` | `OFF` | 启用 C++ 异常 |
| `XNET_USE_RTTI` | `OFF` | 启用 RTTI |

### 本地开发（全部功能）

```bash
cmake -B build \
    -DXNET_BUILD_TESTS=ON \
    -DXNET_BUILD_EXAMPLES=ON \
    -DXNET_BUILD_LINT=ON

cmake --build build
ctest --output-on-failure
cmake --build build --target xnet_lint
```

### 最小化构建（嵌入式目标）

```bash
cmake -B build \
    -DXNET_BUILD_TESTS=OFF \
    -DXNET_BUILD_EXAMPLES=OFF \
    -DXNET_USE_EXCEPTIONS=OFF \
    -DXNET_USE_RTTI=OFF
```

## 🎯 交叉编译

### ESP32 (ESP-IDF)

XNet 可与 ESP-IDF 基于 CMake 的构建系统配合使用。添加为组件：

**选项 A：组件目录**
```bash
mkdir -p components/xnet
cp -r include/ src/ ports/esp32/ CMakeLists.txt components/xnet/
```

在你的组件 CMakeLists.txt 中配置 idf_component_register：

```cmake
idf_component_register(
    SRCS "src/url.cc" "src/buffer.cc" "src/http.cc"
         "src/request.cc" "src/socket_common.cc"
         "ports/esp32/socket_esp32.cc"
    INCLUDE_DIRS "include"
)
```

**选项 B：外部项目**
```cmake
# In your main project CMakeLists.txt
add_subdirectory(xnet)
target_link_libraries(${COMPONENT_LIB} INTERFACE xnet)
```

### ARM / 通用嵌入式

适用于任意 ARM 目标（Cortex-M 等）：

```bash
cmake -B build \
    -DCMAKE_SYSTEM_NAME=Generic \
    -DCMAKE_CXX_COMPILER=arm-none-eabi-g++ \
    -DXNET_USE_EXCEPTIONS=OFF \
    -DXNET_USE_RTTI=OFF
```

## 🧪 运行测试

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

### 测试覆盖率

| 测试文件 | 测试说明 |
|:---------|:--------|
| `test_buffer.cc` | 10 项测试：构造、追加、pop_front、reserve、resize、swap、查找、清空、多次追加 |
| `test_str_view.cc` | 10 项测试：构造、比较、starts_with、查找、substr、to_int、空串 |
| `test_url.cc` | 9 项测试：简单 HTTP、带端口的 HTTPS、查询/片段、用户名/密码、相对路径、IPv6、空 URL、编码/解码、默认端口 |

## 🧼 代码风格

XNet 遵循 [Google C++ 代码风格指南](https://google.github.io/styleguide/cppguide.html)。

```bash
# Check formatting
cmake --build build --target xnet_lint

# Auto-fix formatting
cmake --build build --target xnet_format
```

项目根目录下的 `.clang-format` 文件定义了精确的风格规则。

## 🤖 CI/CD

该项目使用 GitHub Actions，工作流配置文件为 `cmake-multi-platform.yml`：

- **触发条件：** 推送到 `master` / `dev` 分支，向 `master` / `dev` 发起 PR
- **矩阵：**
  - Ubuntu（GCC、Clang）
  - Windows（MSVC）
- **检查项：**
  - ✅ 启用全部功能进行编译
  - ✅ `ctest` — 所有测试通过
  - ✅ `xnet_lint` — 代码风格合规

状态徽章：[![CI](https://github.com/lantubot/xnet/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/lantubot/xnet/actions/workflows/cmake-multi-platform.yml)

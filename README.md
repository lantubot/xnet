
<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue?logo=c%2B%2B" alt="C++20"/>
  <img src="https://img.shields.io/badge/license-MIT-green" alt="MIT"/>
  <img src="https://img.shields.io/badge/STL-free-orange" alt="STL-free"/>
  <img src="https://img.shields.io/badge/ESP32-ready-ff6600" alt="ESP32"/>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20ESP32-lightgrey" alt="Platforms"/>
  <br/>
  <a href="https://github.com/lantubot/xnet/actions"><img src="https://github.com/lantubot/xnet/actions/workflows/cmake-multi-platform.yml/badge.svg" alt="CI"/></a>
</p>

# XNet

面向嵌入式的轻量级 C++20 网络库，**零 STL 依赖**，类似 curl 的 HTTP 客户端。

> 📖 **完整文档参见 [`docs/`](docs/index.md) 目录**

## ✨ 特性

- **零 STL 依赖** — 无 std::vector、std::string，适合嵌入式交叉编译（ESP32、ARM）
- **C++20** — constexpr、无符号运算安全、designated initializers
- **跨平台** — Linux（POSIX）、Windows（WinSock2）、ESP32（LWIP）
- **类 curl API** — HTTP GET/POST/PUT/DELETE，链式调用构建器
- **低编译占用** — `-fno-exceptions -fno-rtti` 友好，适合资源受限环境
- **定宽类型** — 统一使用 `uint32_t`/`int32_t`，ESP32 无需 64 位模拟
- **MIT 许可证** — 自由开源

## 🚀 快速开始

```cpp
#include "xnet/xnet.h"
#include <cstdio>

int main() {
  // 一行 GET
  xnet::Result<xnet::Response> r = xnet::get("http://httpbin.org/get");
  if (r.is_ok()) {
    printf("Status: %d\n", r.value().status_code());
    printf("Body: %s\n", r.value().body());
  }

  // 链式 POST
  const char* json = R"({"hello": "xnet"})";
  xnet::Result<xnet::Response> pr = xnet::Request()
    .url("http://httpbin.org/post")
    .method(xnet::Method::POST)
    .header("Content-Type", "application/json")
    .body(json, 16)
    .timeout(10000)
    .perform();

  if (pr.is_ok()) {
    printf("POST status: %d\n", pr.value().status_code());
  }
  return 0;
}
```

编译运行：
```bash
mkdir build && cd build
cmake .. -DXNET_BUILD_EXAMPLES=ON
cmake --build .
./xnet_http_get
```

## 📦 目录结构

```
xnet/
├── include/xnet/    # 公开头文件
│   ├── xnet.h       # 聚合头文件（包含全部 API）
│   ├── error.h      # Status 枚举、Error、Result<T>
│   ├── str_view.h   # StringView（零分配字符串视图）
│   ├── buffer.h     # Buffer（动态字节缓冲区）
│   ├── url.h        # Url 解析器（RFC 3986）
│   ├── http.h       # HTTP 请求/响应结构
│   ├── socket.h     # Socket 抽象接口 + 工厂
│   └── request.h    # Request 构建器 + Response
├── src/             # 核心实现
├── ports/           # 平台适配层
│   ├── linux/       # POSIX sockets + getaddrinfo
│   ├── windows/     # WinSock2
│   └── esp32/       # LWIP (esp-tls)
├── docs/            # 文档
│   ├── index.md     # 文档首页
│   ├── api.md       # API 参考
│   ├── usage.md     # 使用指南
│   ├── build.md     # 构建指南
│   └── platforms.md # 平台注意事项
├── tests/           # 单元测试（CTest）
├── examples/        # 示例程序
├── .clang-format    # Google C++ 代码风格
└── CMakeLists.txt   # CMake 构建系统
```

## 📖 文档

| 文档 | 说明 |
|:-----|:-----|
| [📚 文档首页](docs/index.md) | 架构概览、设计原则、快速参考 |
| [📐 API 参考](docs/api.md) | 完整 API 文档，覆盖所有模块 |
| [💡 使用指南](docs/usage.md) | 示例代码、使用模式、最佳实践 |
| [🔧 构建指南](docs/build.md) | 构建选项、交叉编译、CI 配置 |
| [🌐 平台注意事项](docs/platforms.md) | Linux / Windows / ESP32 技术细节 |

## 🔧 构建选项

| 选项 | 默认 | 说明 |
|:----|:----:|:-----|
| `XNET_BUILD_TESTS` | ON | 编译并运行单元测试 |
| `XNET_BUILD_EXAMPLES` | ON | 编译示例程序 |
| `XNET_BUILD_LINT` | OFF | 启用 clang-format 代码检查 |
| `XNET_USE_EXCEPTIONS` | OFF | 启用异常（默认关闭） |
| `XNET_USE_RTTI` | OFF | 启用 RTTI（默认关闭） |

### 本地开发（全开）

```bash
cmake .. \
  -DXNET_BUILD_TESTS=ON \
  -DXNET_BUILD_EXAMPLES=ON \
  -DXNET_BUILD_LINT=ON
cmake --build .
ctest --output-on-failure    # 运行测试
cmake --build . --target xnet_lint    # 检查格式
cmake --build . --target xnet_format  # 自动修复格式
```

## 📐 API 概览

### 核心类型

| 类型 | 头文件 | 说明 |
|:----|:------|:-----|
| `xnet::Status` | `error.h` | 错误码枚举（OK / TIMEOUT / DNS_FAILURE 等） |
| `xnet::Error` | `error.h` | 状态码 + 可选描述消息 |
| `xnet::Result<T>` | `error.h` | 标签联合：成功持有 T，失败持有 Error |
| `xnet::StringView` | `str_view.h` | 非拥有字符串视图（constexpr 可用） |
| `xnet::Buffer` | `buffer.h` | 动态字节缓冲区（可移动，不可拷贝） |
| `xnet::Url` | `url.h` | RFC 3986 URL 解析器 |
| `xnet::Method` | `http.h` | HTTP 方法（GET/POST/PUT/DELETE/HEAD/PATCH） |
| `xnet::Version` | `http.h` | HTTP 版本（1.0 / 1.1 / 2.0） |
| `xnet::Header` | `http.h` | HTTP 头部键值对 |
| `xnet::HttpRequest` | `http.h` | HTTP 请求结构（低层） |
| `xnet::HttpResponse` | `http.h` | HTTP 响应解析器 |
| `xnet::Socket` | `socket.h` | TCP Socket 抽象接口 |
| `xnet::SocketFactory` | `socket.h` | 平台相关 Socket 工厂 |
| `xnet::Response` | `request.h` | HTTP 响应（高层接口） |
| `xnet::Request` | `request.h` | HTTP 请求构建器（链式调用） |

### 便利函数

```cpp
// 一行调用
xnet::get(url)         // HTTP GET
xnet::post(url, body, len)  // HTTP POST
xnet::put(url, body, len)   // HTTP PUT
xnet::del(url)         // HTTP DELETE
```

### Response 接口

```cpp
resp.status_code()     // int: 200, 404, 500...
resp.body()            // const char*: 响应体（nullptr 表示无内容）
resp.body_size()       // size_t: 响应体大小
resp.version()         // HTTP 协议版本
resp.header("Name")    // 按名称查找头部，返回 const char*
resp.num_headers()     // 头部数量
resp.header_at(i)      // 按索引访问头部
```

> 📖 **详细的 API 文档请参阅 [`docs/api.md`](docs/api.md)**

## 🌐 平台支持

| 平台 | 状态 | 套接字层 | DNS |
|:----|:----:|:---------|:----|
| Linux | ✅ | POSIX sockets + poll() | getaddrinfo |
| Windows | ✅ | WinSock2 + select() | GetAddrInfoW |
| ESP32 | ✅ | LWIP + esp-tls | lwip_getaddrinfo |

> 📖 **平台技术细节请参阅 [`docs/platforms.md`](docs/platforms.md)**

## 🧪 测试

```bash
cmake .. -DXNET_BUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

测试使用轻量级自定义框架（`test_helpers.h`），无外部依赖。
覆盖 Buffer、StringView、URL 解析的核心功能。

> 📖 **构建和测试详情请参阅 [`docs/build.md`](docs/build.md)**

## 🤝 贡献

1. Fork 仓库并创建特性分支：`git checkout -b feature/my-feature`
2. 提交遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范
3. 确保通过 CI：`cmake --build . --target xnet_lint && ctest`
4. 发起 Pull Request 到 `dev` 分支

## 📄 License

MIT © [XNet Contributors](LICENSE)

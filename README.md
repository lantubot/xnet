# XNet

面向嵌入式的轻量级 C++20 网络库

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 特性

- **零 STL 依赖** — 无 std::vector、std::string，适合嵌入式交叉编译
- **C++20** — 使用 concepts、constexpr、designated initializers 等现代特性
- **跨平台** — Linux、Windows、ESP32
- **类 curl 功能** — HTTP GET/POST/PUT/DELETE，命名更清晰
- **低编译占用** — `-fno-exceptions` / `-fno-rtti` 友好
- **MIT 协议** — 开源自由使用

## 快速开始

```cpp
#include "xnet/xnet.h"

int main() {
    // GET 请求
    xnet::Response resp = xnet::get("https://httpbin.org/get");
    printf("Status: %d\n", resp.status_code());
    printf("Body: %s\n", resp.body());

    // POST 请求
    const char* json = R"({"name": "xnet", "version": 1})";
    xnet::Response resp2 = xnet::post("https://httpbin.org/post", json, 30);

    // 配置化请求
    xnet::Request req;
    req.url("https://api.example.com/data")
       .method(xnet::Method::POST)
       .header("Authorization", "Bearer token123")
       .body(json, 30)
       .timeout(5000);
    xnet::Response resp3 = req.perform();

    return 0;
}
```

## 构建

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## 目录结构

```
xnet/
├── include/xnet/    # 公开头文件
├── src/             # 源码
├── ports/           # 平台适配层
│   ├── linux/
│   ├── windows/
│   └── esp32/
└── examples/        # 示例
```

## 平台支持

| 平台 | 状态 | 传输层 |
|------|------|--------|
| Linux | ✅ | POSIX sockets + epoll |
| Windows | ✅ | WinSock2 |
| ESP32 | ✅ | LWIP (esp-tls) |

## License

MIT

# XNet — 嵌入式轻量级网络库

## 项目概述

面向嵌入式的 C++20 轻量级网络库，实现类 curl 功能，命名更清晰，**零 STL 依赖**。

## 需求分析

### 核心功能
1. **HTTP/HTTPS 客户端** — GET/POST/PUT/DELETE
2. **URL 解析** — protocol/host/port/path/query/fragment
3. **自定义内存管理** — 无 STL，固定缓冲区 + 可选动态分配
4. **跨平台网络层** — Linux(epoll)、Windows(IOCP)、ESP32(LWIP)
5. **简洁 API** — 类 curl 但命名更直观

### 设计原则
- ❌ 不使用 STL（std::vector, std::string, std::map 等）
- ✅ 使用 C++20 语言特性（concepts, constexpr, designated initializers）
- ✅ 零异常或可选异常（`-fno-exceptions` 兼容）
- ✅ 低编译占用，适合交叉编译
- ✅ Header-only 可选编译模型

### API 设计（命名更清晰）
```cpp
// 发起请求
xnet::Response resp = xnet::get("https://api.example.com/data");
xnet::Response resp = xnet::post("https://api.example.com/data", body, len);

// 配置化请求
xnet::Request req;
req.url("https://api.example.com/data")
   .method(xnet::Method::POST)
   .header("Authorization", "Bearer xxx")
   .body(json, len)
   .timeout(5000);

xnet::Response resp = req.perform();

// 响应访问
int code = resp.status_code();       // 200
const char* body = resp.body();      // 响应体
size_t len = resp.body_size();       // 响应体长度
const char* ct = resp.header("content-type");
```

### 目录结构
```
xnet/
├── CMakeLists.txt
├── .gitignore
├── README.md
├── LICENSE
├── include/
│   └── xnet/
│       ├── xnet.h              # 主头文件（包含所有公开API）
│       ├── url.h               # URL 解析器
│       ├── buffer.h            # 内存缓冲区
│       ├── string_view.h       # 字符串视图（零拷贝）
│       ├── http.h              # HTTP 请求/响应构建与解析
│       ├── error.h             # 错误码 & Result 类型
│       ├── socket.h            # 平台抽象接口
│       └── request.h           # 高层 Request / Response API
├── src/
│   ├── url.cpp
│   ├── buffer.cpp
│   ├── http.cpp
│   ├── request.cpp
│   └── socket.cpp              # 默认实现（委托给 ports）
├── ports/
│   ├── linux/
│   │   ├── socket_linux.h
│   │   └── socket_linux.cpp
│   ├── windows/
│   │   ├── socket_windows.h
│   │   └── socket_windows.cpp
│   └── esp32/
│       ├── socket_esp32.h
│       └── socket_esp32.cpp
└── examples/
    └── http_get.cpp
```

### 编译目标
- 最小化二进制体积（< 50KB 静态库）
- 无 libstdc++ 链接依赖（`-nostdlib` 友好）
- CMake 3.16+，C++20
- MIT 协议开源

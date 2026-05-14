# 平台说明

## 🌐 平台概览

| 平台 | Socket 后端 | DNS | 状态 |
|:---------|:---------------|:----|:------:|
| Linux | POSIX sockets + `poll()` | `getaddrinfo()` | ✅ 生产就绪 |
| Windows | WinSock2 + `select()` | `getaddrinfo()` | ✅ 生产就绪 |
| ESP32 | LWIP BSD sockets | `gethostbyname()` | ✅ 已在 ESP-IDF 上验证 |

---

## 🐧 Linux

### 实现细节

- **非阻塞 I/O：** 通过 `fcntl()` 配合 `O_NONBLOCK` 实现
- **连接超时：** 非阻塞 `connect()` + `poll()` 监视 `POLLOUT`
- **发送/接收：** `poll()` 使用无限超时；`MSG_NOSIGNAL` 防止 `SIGPIPE`
- **线程安全：** Socket 实例非线程安全；请在每个线程中使用独立的 socket
- **DNS：** `getaddrinfo()` 是阻塞调用（标准 POSIX 行为）

### 依赖项

- `libc`（标准 POSIX）
- `libpthread`（通过 CMake 自动链接）
- 无外部依赖

### 已知问题

| 问题 | 解决方法 |
|:------|:-----------|
| `getaddrinfo()` 阻塞 DNS 解析 | 设置较短的应用层超时 |
| 不支持 TLS/SSL | 计划在后续版本中实现 |

---

## 🪟 Windows

### 实现细节

- **WSA 初始化：** 在首次创建 socket 时延迟调用 `WSAStartup(2, 2)`（通过 `InterlockedExchange` 保证线程安全）
- **非阻塞 I/O：** 在 connect 前调用 `ioctlsocket(FIONBIO)`
- **连接超时：** 非阻塞 `connect()` + `select()` 监视 write/except 文件描述符集
- **发送：** 阻塞循环，遇到 `WSAEWOULDBLOCK` 时重试
- **接收：** `select()` 使用 30 秒超时，然后执行阻塞 `recv()`
- **关闭：** `shutdown(SD_BOTH)` + `closesocket()`

### 依赖项

- `ws2_32.lib`（通过 CMake 自动链接）
- 来自 `ws2tcpip.h` 的 `getaddrinfo()`

### 已知问题

| 问题 | 解决方法 |
|:------|:-----------|
| `select()` 最多只能处理 `FD_SETSIZE` 个 socket | 客户端场景下可正常使用 |
| 无 `MSG_NOSIGNAL` 等效机制 | 通过 `WSAECONNRESET` 处理错误 |
| Winsock 必须按进程初始化 | 在首次调用 `SocketFactory::create()` 时自动初始化 |

---

## 📡 ESP32 (ESP-IDF)

### 实现细节

- **Socket 创建：** 通过 LWIP 调用标准 `socket(AF_INET, SOCK_STREAM, 0)`
- **超时设置：** 使用 `setsockopt()` 配合 `SO_RCVTIMEO` / `SO_SNDTIMEO`
- **DNS：** `gethostbyname()`（LWIP 内置解析器）
- **日志：** 通过 `ESP_LOGD` / `ESP_LOGE` / `ESP_LOGW` 使用 ESP-IDF 日志系统
- **内存：** 使用 `new (std::nothrow)` — 在分配失败时进行 nullptr 检查
- **错误映射：** 将 `errno` 映射为 `xnet::Status`，包含 ESP-IDF 特定的错误模式

### ESP-IDF 配置

所需的 `sdkconfig` 设置：

```
CONFIG_LWIP_SO_RCVTIMEO=y
CONFIG_LWIP_SO_SNDTIMEO=y
```

### 内存注意事项

- **避免使用 64 位类型** — 所有内部类型均为 `uint32_t` / `int32_t`
- **无 STL** — 避免 `std::string`、`std::vector` 造成堆碎片
- **无异常** — `XNET_USE_EXCEPTIONS=OFF` 可节省约 20KB Flash
- **无 RTTI** — `XNET_USE_RTTI=OFF` 可节省类型信息开销
- **缓冲区增长策略：** 从小容量开始，扩容时翻倍 — 使用 `reserve()` 可进行可预测的内存分配

### 典型 Flash/RAM 占用

| 组件 | 约计 Flash | 约计 RAM |
|:----------|:-------------:|:-----------:|
| XNet 库（全部模块） | ~15 KB | ~0.5 KB (BSS) |
| 每个 `Buffer`（空） | — | 24 字节 |
| 每个 socket 实例 | — | ~8 字节 |
| Request/Response（峰值） | — | ~4 KB + 请求体 |

> 备注：使用 ESP32 GCC 编译选项 `-Os`、`-fno-exceptions`、`-fno-rtti` 测得。

---

## 🔄 平台选择

平台在 CMake 配置阶段自动检测：

```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # → ports/linux/socket_linux.cc
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # → ports/windows/socket_windows.cc
elseif(CMAKE_SYSTEM_NAME STREQUAL "Generic")  # ESP32
    # → ports/esp32/socket_esp32.cc
endif()
```

如需手动覆盖，可设置 `CMAKE_SYSTEM_NAME`：

```bash
cmake -B build -DCMAKE_SYSTEM_NAME=Generic
```

## 🔜 计划支持的平台

- **macOS** — 基于 FreeBSD 的 POSIX 系统，与 Linux 端口非常相似
- **ARM mbed OS** — 自定义 socket 抽象层
- **Zephyr RTOS** — 通过 Zephyr 网络协议栈提供 BSD sockets

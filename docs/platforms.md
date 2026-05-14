# Platform Notes

## 🌐 Platform Overview

| Platform | Socket Backend | DNS | Status |
|:---------|:---------------|:----|:------:|
| Linux | POSIX sockets + `poll()` | `getaddrinfo()` | ✅ Production-ready |
| Windows | WinSock2 + `select()` | `getaddrinfo()` | ✅ Production-ready |
| ESP32 | LWIP BSD sockets | `gethostbyname()` | ✅ Verified on ESP-IDF |

---

## 🐧 Linux

### Implementation Details

- **Non-blocking I/O** via `fcntl()` with `O_NONBLOCK`
- **Connect timeout:** non-blocking `connect()` + `poll()` on `POLLOUT`
- **Send/recv:** `poll()` with infinite timeout; `MSG_NOSIGNAL` to prevent `SIGPIPE`
- **Thread safety:** Socket instances are not thread-safe; use one socket per thread
- **DNS:** `getaddrinfo()` is blocking (standard POSIX behavior)

### Dependencies

- `libc` (standard POSIX)
- `libpthread` (linked automatically via CMake)
- No external dependencies

### Known Issues

| Issue | Workaround |
|:------|:-----------|
| `getaddrinfo()` blocks DNS resolution | Set shorter application-level timeouts |
| No TLS/SSL support | Planned for future releases |

---

## 🪟 Windows

### Implementation Details

- **WSA Startup:** `WSAStartup(2, 2)` called lazily on first socket creation (thread-safe via `InterlockedExchange`)
- **Non-blocking I/O:** `ioctlsocket(FIONBIO)` before connect
- **Connect timeout:** non-blocking `connect()` + `select()` on write/except file descriptor sets
- **Send:** blocking loop with `WSAEWOULDBLOCK` retry
- **Recv:** `select()` with 30-second timeout, then blocking `recv()`
- **Shutdown:** `shutdown(SD_BOTH)` + `closesocket()`

### Dependencies

- `ws2_32.lib` (linked automatically via CMake)
- `getaddrinfo()` from `ws2tcpip.h`

### Known Issues

| Issue | Workaround |
|:------|:-----------|
| `select()` only handles up to `FD_SETSIZE` sockets | Fine for client-side usage |
| No `MSG_NOSIGNAL` equivalent | Errors handled via `WSAECONNRESET` |
| Winsock must be initialized per process | Auto-initialized on first `SocketFactory::create()` |

---

## 📡 ESP32 (ESP-IDF)

### Implementation Details

- **Socket creation:** Standard `socket(AF_INET, SOCK_STREAM, 0)` via LWIP
- **Timeouts:** `setsockopt()` with `SO_RCVTIMEO` / `SO_SNDTIMEO`
- **DNS:** `gethostbyname()` (LWIP built-in resolver)
- **Logging:** ESP-IDF logging system via `ESP_LOGD` / `ESP_LOGE` / `ESP_LOGW`
- **Memory:** Uses `new (std::nothrow)` — nullptr check on allocation failure
- **Error mapping:** `errno` → `xnet::Status` with ESP-IDF specific patterns

### ESP-IDF Configuration

Required `sdkconfig` settings:

```
CONFIG_LWIP_SO_RCVTIMEO=y
CONFIG_LWIP_SO_SNDTIMEO=y
```

### Memory Considerations

- **62-bit types avoided** — all internal types are `uint32_t`/`int32_t`
- **No STL** — no `std::string`, `std::vector` heap fragmentation
- **No exceptions** — `XNET_USE_EXCEPTIONS=OFF` saves ~20KB flash
- **No RTTI** — `XNET_USE_RTTI=OFF` saves typeinfo overhead
- **Buffer growth:** starts small, doubles on expansion — use `reserve()` for predictable allocation

### Typical Flash/RAM Usage

| Component | Approx. Flash | Approx. RAM |
|:----------|:-------------:|:-----------:|
| XNet library (all modules) | ~15 KB | ~0.5 KB (BSS) |
| Per `Buffer` (empty) | — | 24 bytes |
| Per socket instance | — | ~8 bytes |
| Request/Response (peak) | — | ~4 KB + body |

> Note: Measurements with ESP32 GCC `-Os`, `-fno-exceptions`, `-fno-rtti`.

---

## 🔄 Platform Selection

Platform is auto-detected at CMake configure time:

```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # → ports/linux/socket_linux.cc
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # → ports/windows/socket_windows.cc
elseif(CMAKE_SYSTEM_NAME STREQUAL "Generic")  # ESP32
    # → ports/esp32/socket_esp32.cc
endif()
```

To override, set `CMAKE_SYSTEM_NAME` manually:

```bash
cmake -B build -DCMAKE_SYSTEM_NAME=Generic
```

## 🔜 Planned Platform Support

- **macOS** — FreeBSD-derived POSIX, very similar to Linux port
- **ARM mbed OS** — Custom socket abstraction layer
- **Zephyr RTOS** — BSD sockets via Zephyr networking stack

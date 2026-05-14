# XNet API 参考

> **头文件：** `#include "xnet/xnet.h"`（总包含）或单个头文件。
> **命名空间：** `xnet`

---

## 📁 核心类型 — `error.h`

### `enum class Status : uint8_t`

库中使用的错误码。

| 枚举值 | 描述 |
|:-----------|:------------|
| `OK` | 成功 |
| `UNKNOWN` | 未指定的错误 |
| `INVALID_ARGUMENT` | 无效输入（null/空/越界） |
| `NOT_FOUND` | 资源未找到 |
| `TIMEOUT` | 连接或 I/O 超时 |
| `CONNECTION_REFUSED` | 远程对端拒绝连接 |
| `CONNECTION_RESET` | 对端重置连接 |
| `DNS_FAILURE` | 主机名解析失败 |
| `PROTOCOL_ERROR` | HTTP 协议违规 |
| `OUT_OF_MEMORY` | 内存分配失败 |
| `UNSUPPORTED_PROTOCOL` | 不支持的协议或地址族 |
| `SSL_ERROR` | TLS/SSL 握手或加密错误 |
| `IO_ERROR` | 常规 I/O 错误 |

```cpp
const char* msg = xnet::to_string(Status::TIMEOUT);  // "TIMEOUT"
```

### `struct Error`

一个状态码与可选的、人类可读的消息配对。

```cpp
xnet::Error err(xnet::Status::DNS_FAILURE, "host not found");
const char* msg = err.what();  // "host not found"（回退到 to_string(code)）
```

- `code` — `Status` 值
- `message` — `const char*`，可为 `nullptr`
- `what()` — 如果有消息则返回消息，否则返回 `to_string(code)`

### `template<typename T> class Result<T>`

一个标签联合体，持有类型 `T` 的值（成功）或 `Error`（失败）。

**工厂方法：**

| 方法 | 描述 |
|:-------|:------------|
| `Result::ok(const T& val)` | 构造一个成功结果（`const&`） |
| `Result::ok(T&& val)` | 构造一个成功结果（移动） |
| `Result::err(const Error& e)` | 构造一个错误结果 |

**查询：**

| 方法 | 描述 |
|:-------|:------------|
| `is_ok()` | 如果持有值则返回 `true` |
| `is_err()` | 如果持有错误则返回 `true` |

**访问器**（前置条件：必须与状态匹配）：

| 方法 | 前置条件 | 返回值 |
|:-------|:-------------|:--------|
| `value()` | `is_ok()` | `T&` 或 `const T&` |
| `error()` | `is_err()` | `Error&` 或 `const Error&` |

**生命周期：** 可复制、可移动。析构函数调用活动成员对应的析构函数。

```cpp
xnet::Result<int> r = xnet::Result<int>::ok(42);
if (r.is_ok()) {
    int v = r.value();  // 42
}

xnet::Result<int> e = xnet::Result<int>::err(
    xnet::Error(xnet::Status::NOT_FOUND));
if (e.is_err()) {
    xnet::Error err = e.error();
}
```

### `template<typename T> struct RemoveReference<T>`

类型萃取，移除类型中的 `&` 和 `&&`。内部由 `move()` 使用。

### `template<typename T> T&& move(T& t) noexcept`

将 `t` 转换为右值引用，启用移动语义。替代 `std::move`。

---

## 📁 字符串视图 — `str_view.h`

### `class StringView`

一个轻量级的、非拥有性的连续字符序列引用。
- 无内存分配、无拷贝、无所有权
- 所有操作尽可能为 `constexpr`
- 调用者必须确保引用的数据寿命超过 `StringView`

**常量：**

| 成员 | 值 |
|:-------|:------|
| `StringView::npos` | `size_t(-1)` — "未找到"的哨兵值 |

**构造函数：**

| 签名 | 描述 |
|:----------|:------------|
| `StringView()` | 空视图（`data() == nullptr`，`size() == 0`） |
| `StringView(const char* s)` | 从空终止字符串构造。接受 `nullptr` → 空视图。 |
| `StringView(const char* s, size_t len)` | 从指针 + 显式长度构造。不以 null 结尾。 |
| `StringView(nullptr_t)` | 显式 nullptr → 空视图 |

**观察器：**

| 方法 | 描述 |
|:-------|:------------|
| `data()` | 指向字符数据的原始指针（不保证以 null 结尾） |
| `size()`、`length()` | 字符数量 |
| `empty()` | 如果 `size() == 0` 则返回 `true` |
| `operator[](size_t i)` | 索引 `i` 处的字符（无边界检查，越界为未定义行为） |

**比较：**

| 方法 | 描述 |
|:-------|:------------|
| `operator==(StringView, StringView)` | 逐字节比较相等性 |
| `operator!=(StringView, StringView)` | `==` 的否定 |

**搜索：**

| 方法 | 描述 |
|:-------|:------------|
| `starts_with(const char* prefix)` | 如果视图以 `prefix` 开头返回 `true` |
| `find(const char* s, size_t pos = 0)` | 子串 `s` 在 `pos` 或之后首次出现的位置。未找到则返回 `npos`。 |
| `find(char c, size_t pos = 0)` | 字符 `c` 在 `pos` 或之后首次出现的位置。未找到则返回 `npos`。 |

**子串：**

| 方法 | 描述 |
|:-------|:------------|
| `substr(size_t offset, size_t count = npos)` | 从 `offset` 开始的子串，最多 `count` 个字符。自动裁剪到可用长度。如果 `offset >= size()` 则返回空视图。 |

**工具：**

| 方法 | 描述 |
|:-------|:------------|
| `to_int()` | 解析十进制整数。格式错误时返回 0。溢出时限制为 `INT_MAX`/`INT_MIN`。 |
| `hash()` | FNV-1a 哈希。根据平台自动选择 64 位或 32 位参数。 |

```cpp
constexpr xnet::StringView sv("hello world");
constexpr bool ok = sv.starts_with("hello");       // true
constexpr size_t p = sv.find('w');                  // 6
constexpr xnet::StringView sub = sv.substr(0, 5);   // "hello"
constexpr int n = xnet::StringView("42").to_int();  // 42
```

---

## 📁 缓冲区 — `buffer.h`

### `class Allocator`（抽象类）

内存分配接口。实现此接口以提供自定义分配器（池、 arena、静态）。

```cpp
class Allocator {
    virtual void* allocate(size_t size) = 0;
    virtual void deallocate(void* ptr, size_t size) = 0;
};
```

### `class MallocAllocator`

使用 `malloc()`/`free()` 的默认 `Allocator` 实现。

### `class Buffer`

一个可调整大小的字节缓冲区，拥有其存储空间。仅可移动 — 无隐式拷贝。

**常量：**

| 成员 | 值 |
|:-------|:------|
| `Buffer::npos` | `size_t(-1)` — "未找到"的哨兵值 |

**构造：**

| 签名 | 描述 |
|:----------|:------------|
| `Buffer()` | 空缓冲区，使用默认的 `MallocAllocator` |
| `explicit Buffer(Allocator* allocator)` | 使用自定义分配器的空缓冲区 |
| `Buffer(Buffer&& other)` | 移动构造函数 — 转移所有权 |
| `Buffer(const Buffer&)` | **已删除** — 请改用 `clone()` |

**观察器：**

| 方法 | 描述 |
|:-------|:------------|
| `data()` | 指向数据的原始指针（`const char*` 或 `char*`） |
| `mutable_data()` | `char*` 可变指针 |
| `size()` | 已写入的字节数 |
| `capacity()` | 已分配的容量 |
| `empty()` | 如果 `size() == 0` 则返回 `true` |
| `clear()` | 将 `size()` 重置为 0（不释放内存） |

**元素访问：**

| 方法 | 描述 |
|:-------|:------------|
| `operator[](size_t)` | 索引处的字节（无边界检查） |

**修改器：**

| 方法 | 描述 |
|:-------|:------------|
| `append(const void* data, size_t len)` | 追加原始字节。需要时自动预留空间。 |
| `append(const Buffer& other)` | 从另一个缓冲区追加 |
| `append(char byte)` | 追加单个字节 |
| `pop_front(size_t n)` | 移除前 `n` 个字节。基于 `memmove` 的高效实现。 |
| `resize(size_t new_size)` | 调整大小。增长时零初始化，缩减时截断。 |
| `reserve(size_t new_capacity)` | 预分配容量。如果请求容量 <= 当前容量则无操作。 |
| `swap(Buffer& other)` | 与另一个缓冲区交换内容（O(1)，无内存分配）。 |

**搜索：**

| 方法 | 描述 |
|:-------|:------------|
| `find(const char* needle, size_t needle_len)` | 查找 `needle` 首次出现的位置。返回偏移量或 `npos`。 |

**克隆：**

| 方法 | 描述 |
|:-------|:------------|
| `clone()` | 使用相同分配器进行深拷贝 |

```cpp
xnet::Buffer buf;
buf.append("Hello, ", 7);
buf.append("World!", 6);
XNET_ASSERT(buf.size() == 13);
```

---

## 📁 URL — `url.h`

### `class Url`

RFC 3986 URL 解析器。所有组件都是对原始输入字符串的 `StringView` 引用。
解析过程中无堆内存分配。

**字段：**

| 字段 | 类型 | 描述 |
|:------|:-----|:------------|
| `scheme` | `StringView` | 协议方案（`"http"`、`"https"`，缺失时为空） |
| `host` | `StringView` | 主机名或 IP（IPv6 不含方括号，如 `"::1"`） |
| `path` | `StringView` | 路径组件（`"/index.html"`，缺失时为空） |
| `query` | `StringView` | 查询字符串（`"q=hello&n=1"`，缺失时为空） |
| `fragment` | `StringView` | 片段标识符（`"section-2"`，缺失时为空） |
| `username` | `StringView` | 用户信息中的用户名（缺失时为空） |
| `password` | `StringView` | 用户信息中的密码（缺失时为空） |
| `port` | `int` | 端口号。`0` 表示"未指定"。 |

**静态方法：**

| 方法 | 描述 |
|:-------|:------------|
| `parse(const char* str, size_t len)` | 解析 URL。返回 `Result<Url>`。接受绝对路径（`http://...`）、协议相对路径（`//host/path`）、路径相对（`/path`）、仅查询（`?q=1`）、仅片段（`#frag`）。 |
| `encode(const StringView& input, char* output, size_t output_size, size_t* written)` | 百分比编码。非保留字符（`A-Z a-z 0-9 - . _ ~`）原样通过；其余变为 `%XX`。 |
| `decode(const StringView& input, char* output, size_t output_size, size_t* written)` | 百分比解码。`%XX` 序列被解码；`+` 变为空格。 |

```cpp
xnet::Result<xnet::Url> r = xnet::Url::parse("https://user:pass@host:443/path?q=1#frag", 38);
if (r.is_ok()) {
    const xnet::Url& u = r.value();
    // u.scheme == "https"
    // u.host   == "host"
    // u.port   == 443
    // u.path   == "/path"
}
```

---

## 📁 HTTP — `http.h`

### `enum class Method : uint8_t`

| 枚举值 | 线路格式 |
|:-----------|:------------|
| `GET` | `"GET"` |
| `POST` | `"POST"` |
| `PUT` | `"PUT"` |
| `DELETE` | `"DELETE"` |
| `HEAD` | `"HEAD"` |
| `PATCH` | `"PATCH"` |

`to_string(Method)` 返回线路格式字符串。

### `enum class Version : uint8_t`

| 枚举值 | 线路格式 |
|:-----------|:------------|
| `HTTP_1_0` | `"HTTP/1.0"` |
| `HTTP_1_1` | `"HTTP/1.1"` |
| `HTTP_2_0` | `"HTTP/2.0"` |

`to_string(Version)` 返回线路格式字符串。

### `struct Header`

| 字段 | 类型 | 描述 |
|:------|:-----|:------------|
| `name` | `StringView` | 头部名称（如 `"Content-Type"`） |
| `value` | `StringView` | 头部值（如 `"application/json"`） |

### `struct HttpRequest`

底层 HTTP 请求表示。可直接使用，或通过 `Request` 构建器使用。

| 字段 | 类型 | 描述 |
|:------|:-----|:------------|
| `method` | `Method` | HTTP 方法 |
| `url` | `StringView` | 请求路径/URL |
| `version` | `Version` | HTTP 版本（默认：1.1） |
| `headers` | `Header[kMaxHeaders]` | 头部数组（最多 64 个） |
| `num_headers` | `size_t` | 有效头部数量 |
| `body` | `Buffer*` | 指向请求体的非拥有指针（可为 `nullptr`） |

| 方法 | 描述 |
|:-------|:------------|
| `serialize(Buffer& out)` | 序列化为线路格式（`METHOD path HTTP/1.1\r\n...`） |

### `struct HttpResponse`

解析后的 HTTP 响应。`parse()` 将头部 `StringView` 引用存储在提供的 `header_storage` 缓冲区中。

| 字段 | 类型 | 描述 |
|:------|:-----|:------------|
| `version` | `Version` | HTTP 版本 |
| `status_code` | `int` | 状态码（200、404 等） |
| `reason` | `StringView` | 原因短语（如 "OK"、"Not Found"） |
| `headers` | `Header[kMaxHeaders]` | 解析后的头部 |
| `num_headers` | `size_t` | 已解析的头部数量 |
| `body` | `Buffer` | 响应体（拥有所有权） |

| 方法 | 描述 |
|:-------|:------------|
| `static parse(const char* data, size_t len, Buffer& header_storage)` | 解析 HTTP 响应。返回 `Result<HttpResponse>`。 |

---

## 📁 Socket — `socket.h`

### `struct SocketError : Error`

便捷工厂，用于创建与 socket 相关的错误。

```cpp
xnet::SocketError::connection_refused("port 80 closed");
xnet::SocketError::timeout("connect timed out");
xnet::SocketError::dns_failure("host not found");
```

### `class Socket`（抽象类）

TCP socket I/O 接口。

| 方法 | 描述 |
|:-------|:------------|
| `connect(const char* host, int port, int timeout_ms)` | 连接到 `host:port`。`timeout_ms <= 0` 表示无限等待。 |
| `send(const void* data, size_t len)` | 发送字节。返回实际发送的字节数或错误。 |
| `recv(void* buf, size_t max_len)` | 接收字节。EOF（对端关闭）时返回 0。 |
| `close()` | 关闭 socket。幂等操作（可安全多次调用）。 |

### `class SocketFactory`

创建平台特定 `Socket` 实例的静态工厂。

| 方法 | 描述 |
|:-------|:------------|
| `create(const char* host, int port)` | 创建一个平台 TCP socket（未连接）。返回 `Result<Socket*>`。 |
| `destroy(Socket* s)` | 销毁 socket。自动关闭。`nullptr` 安全。 |

---

## 📁 请求 / 响应 — `request.h`

### `struct Response`

拥有存储空间的高层 HTTP 响应。

| 方法 | 描述 |
|:-------|:------------|
| `status_code()` | HTTP 状态码（`int`） |
| `body()` | 响应体（`const char*` 或 `nullptr`）。生命周期与 `Response` 绑定。 |
| `body_size()` | 响应体大小（`size_t`） |
| `version()` | HTTP 版本 |
| `header(const char* name)` | 按名称查找头部（不区分大小写）。返回 `const char*` 或 `nullptr`。 |
| `num_headers()` | 头部数量 |
| `header_at(size_t i)` | 索引 `i` 处的头部 |

### `class Request`

构建器模式的 HTTP 请求。默认超时：30 秒。

**构建器方法**（均返回 `Request&` 以支持链式调用）：

| 方法 | 描述 |
|:-------|:------------|
| `url(const char* url)` | 设置请求 URL。内部拷贝。 |
| `method(Method m)` | 设置 HTTP 方法 |
| `header(const char* name, const char* value)` | 添加头部。名称和值在内部拷贝。 |
| `body(const char* data, size_t len)` | 设置请求体。内部拷贝。 |
| `timeout(int ms)` | 设置超时时间（毫秒）。`<= 0` 表示无超时。 |

**执行：**

| 方法 | 描述 |
|:-------|:------------|
| `perform()` | 执行请求。解析 URL → 连接 → 发送 → 接收 → 解析响应。返回 `Result<Response>`。 |

### 自由函数

```cpp
xnet::get("http://example.com/");
xnet::post("http://example.com/", json, json_len);
xnet::put("http://example.com/", body, body_len);
xnet::del("http://example.com/");
```

### 头部查找语义

- 根据 RFC 7230 不区分大小写匹配
- 缺失头部返回 `nullptr`
- 返回的 `const char*` 以 null 结尾（解析器向存储追加 `\0`）
- 生命周期与 `Response` 对象绑定

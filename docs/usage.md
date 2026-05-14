# XNet 使用指南

## 目录

- [基础 HTTP GET](#basic-http-get)
- [构建器模式](#builder-pattern)
- [处理请求头](#working-with-headers)
- [错误处理](#error-handling)
- [URL 操作](#url-manipulation)
- [缓冲区操作](#buffer-operations)
- [StringView 操作](#stringview-operations)
- [内存管理](#working-with-memory)

---

## 基础 HTTP GET

```cpp
#include "xnet/xnet.h"
#include <cstdio>

int main() {
    // Simplest form — one-liner
    xnet::Result<xnet::Response> r = xnet::get("http://httpbin.org/get");

    if (r.is_ok()) {
        printf("Status: %d\n", r.value().status_code());
        printf("Body (%zu bytes): %s\n",
               r.value().body_size(),
               r.value().body() ? r.value().body() : "");
    } else {
        printf("Error: %s\n", r.error().what());
    }
    return 0;
}
```

## 构建器模式

如需对请求进行完全控制，可使用 `Request` 构建器：

```cpp
const char* json = R"({"hello": "xnet"})";

xnet::Result<xnet::Response> r = xnet::Request()
    .url("http://httpbin.org/post")
    .method(xnet::Method::POST)
    .header("Content-Type", "application/json")
    .header("Accept", "application/json")
    .body(json, 16)
    .timeout(10000)  // 10s timeout
    .perform();
```

构建器支持所有常见 HTTP 方法：

```cpp
// PUT with body
xnet::put("http://example.com/resource/1", body_data, body_len);

// DELETE
xnet::del("http://example.com/resource/1");

// HEAD
xnet::Request().url("http://example.com/")
    .method(xnet::Method::HEAD)
    .perform();
```

## 处理请求头

**设置请求头：**

```cpp
xnet::Request req;
req.url("http://api.example.com/data")
   .header("Authorization", "Bearer <token>")
   .header("User-Agent", "xnet/0.1.0")
   .header("Content-Type", "application/json");
```

- 请求头会在内部拷贝——外部字符串可以是临时变量
- 最多支持 64 个请求头（`HttpRequest::kMaxHeaders`）
- 如果未设置 `Host` 请求头，会自动添加
- 如果设置了请求体，`Content-Length` 会自动计算

**读取响应头：**

```cpp
xnet::Result<xnet::Response> r = xnet::get("http://httpbin.org/get");
if (r.is_ok()) {
    const xnet::Response& resp = r.value();

    // Case-insensitive lookup
    const char* ct = resp.header("content-type");  // works
    const char* ct2 = resp.header("Content-Type");  // also works

    // Iterate all headers
    for (size_t i = 0; i < resp.num_headers(); ++i) {
        printf("%s: %s\n",
               resp.header_at(i).name.data(),
               resp.header_at(i).value.data());
    }
}
```

## 错误处理

XNet 使用 `Result<T>` 进行错误处理——不使用异常。

```cpp
xnet::Result<xnet::Response> r = xnet::Request()
    .url("http://example.com/")
    .timeout(3000)
    .perform();

if (r.is_ok()) {
    use(r.value());
} else {
    xnet::Error err = r.error();
    switch (err.code) {
        case xnet::Status::TIMEOUT:
            printf("Request timed out\n");
            break;
        case xnet::Status::DNS_FAILURE:
            printf("DNS resolution failed: %s\n", err.message);
            break;
        case xnet::Status::CONNECTION_REFUSED:
            printf("Connection refused\n");
            break;
        case xnet::Status::PROTOCOL_ERROR:
            printf("Protocol error: %s\n", err.what());
            break;
        default:
            printf("Error: %s\n", err.what());
            break;
    }
}
```

## URL 操作

**解析 URL：**

```cpp
const char* url_str = "https://user:secret@api.example.com:8443/search?q=hello&n=1#results";

xnet::Result<xnet::Url> r = xnet::Url::parse(url_str, 58);
if (r.is_ok()) {
    const xnet::Url& url = r.value();
    // url.scheme   == "https"
    // url.username == "user"
    // url.password == "secret"
    // url.host     == "api.example.com"
    // url.port     == 8443
    // url.path     == "/search"
    // url.query    == "q=hello&n=1"
    // url.fragment == "results"
}
```

**百分比编码/解码：**

```cpp
// Encode
char encoded[256];
size_t written;
xnet::Url::encode(xnet::StringView("hello world"), encoded, sizeof(encoded), &written);
// encoded == "hello%20world"

// Decode
char decoded[256];
xnet::Url::decode(xnet::StringView("hello%20world%21"), decoded, sizeof(decoded), &written);
// decoded == "hello world!"
```

## 缓冲区操作

`Buffer` 是主要的字节容器，追加数据时会自动扩容。

```cpp
xnet::Buffer buf;

// Append data
buf.append("GET /index.html HTTP/1.1\r\n", 27);
buf.append("Host: example.com\r\n", 19);
buf.append('\r');
buf.append('\n');

// Use directly
send_socket(sock, buf.data(), buf.size());

// Deep copy
xnet::Buffer copy = buf.clone();

// Move ownership
xnet::Buffer other = static_cast<xnet::Buffer&&>(buf);

// Pre-allocate for performance
buf.reserve(4096);

// Reset (doesn't free memory)
buf.clear();
```

## StringView 操作

`StringView` 在整个 API 中广泛使用，以避免字符串拷贝。

```cpp
// Construction
xnet::StringView sv1;                      // empty
xnet::StringView sv2("hello");              // from const char*
xnet::StringView sv3(data, length);         // from ptr + len

// Comparison
if (sv2 == xnet::StringView("hello")) { /* true */ }

// Searching
size_t pos = sv2.find('e');     // → 1
size_t pos2 = sv2.find("ll");   // → 2

// Substring
xnet::StringView sub = sv2.substr(1, 3);  // "ell"

// Parse integer
int val = xnet::StringView("42").to_int();  // 42
```

## 内存管理

XNet 专为资源受限环境设计。关键内存模式如下：

1. **无隐式分配**——`StringView` 从不分配内存
2. **显式容量**——使用 `Buffer::reserve(N)` 实现可预测的内存管理
3. **自定义分配器**——实现 `Allocator` 以使用池/区域分配策略：

```cpp
class StaticPool : public xnet::Allocator {
    char pool_[4096];
    size_t used_ = 0;
public:
    void* allocate(size_t size) override {
        if (used_ + size > sizeof(pool_)) return nullptr;
        void* ptr = pool_ + used_;
        used_ += size;
        return ptr;
    }
    void deallocate(void*, size_t) override {
        // no-op for linear allocator
    }
};

StaticPool pool;
xnet::Buffer buf(&pool);
```

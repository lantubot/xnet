# XNet Usage Guide

## Table of Contents

- [Basic HTTP GET](#basic-http-get)
- [Builder Pattern](#builder-pattern)
- [Working with Headers](#working-with-headers)
- [Error Handling](#error-handling)
- [URL Manipulation](#url-manipulation)
- [Buffer Operations](#buffer-operations)
- [StringView Operations](#stringview-operations)
- [Working with Memory](#working-with-memory)

---

## Basic HTTP GET

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

## Builder Pattern

For full control over the request, use the `Request` builder:

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

The builder supports all common HTTP methods:

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

## Working with Headers

**Setting request headers:**

```cpp
xnet::Request req;
req.url("http://api.example.com/data")
   .header("Authorization", "Bearer <token>")
   .header("User-Agent", "xnet/0.1.0")
   .header("Content-Type", "application/json");
```

- Headers are copied internally — external strings can be temporary
- Max 64 headers (`HttpRequest::kMaxHeaders`)
- `Host` header is auto-added if not set
- `Content-Length` is auto-computed if body is set

**Reading response headers:**

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

## Error Handling

XNet uses `Result<T>` for error handling — no exceptions.

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

## URL Manipulation

**Parse a URL:**

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

**Percent encode/decode:**

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

## Buffer Operations

`Buffer` is the primary byte container. It grows automatically on append.

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

## StringView Operations

`StringView` is used throughout the API to avoid string copies.

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

## Working with Memory

XNet is designed for constrained environments. Key memory patterns:

1. **No implicit allocations** — `StringView` never allocates
2. **Explicit capacity** — `Buffer::reserve(N)` for predictable memory
3. **Custom allocators** — Implement `Allocator` for pool/arena strategies:

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

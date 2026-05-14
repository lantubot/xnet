# XNet API Reference

> **Header:** `#include "xnet/xnet.h"` (umbrella) or individual headers.
> **Namespace:** `xnet`

---

## 📁 Core Types — `error.h`

### `enum class Status : uint8_t`

Error codes used throughout the library.

| Enumerator | Description |
|:-----------|:------------|
| `OK` | Success |
| `UNKNOWN` | Unspecified error |
| `INVALID_ARGUMENT` | Invalid input (null/empty/out-of-range) |
| `NOT_FOUND` | Resource not found |
| `TIMEOUT` | Connection or I/O timed out |
| `CONNECTION_REFUSED` | Remote peer refused connection |
| `CONNECTION_RESET` | Connection reset by peer |
| `DNS_FAILURE` | Hostname resolution failed |
| `PROTOCOL_ERROR` | HTTP protocol violation |
| `OUT_OF_MEMORY` | Allocation failure |
| `UNSUPPORTED_PROTOCOL` | Unsupported protocol or address family |
| `SSL_ERROR` | TLS/SSL handshake or encryption error |
| `IO_ERROR` | General I/O failure |

```cpp
const char* msg = xnet::to_string(Status::TIMEOUT);  // "TIMEOUT"
```

### `struct Error`

A status code paired with an optional human-readable message.

```cpp
xnet::Error err(xnet::Status::DNS_FAILURE, "host not found");
const char* msg = err.what();  // "host not found" (falls back to to_string(code))
```

- `code` — `Status` value
- `message` — `const char*`, may be `nullptr`
- `what()` — returns `message` if set, otherwise `to_string(code)`

### `template<typename T> class Result<T>`

A tagged union that holds either a value of type `T` (success) or an `Error` (failure).

**Factory methods:**

| Method | Description |
|:-------|:------------|
| `Result::ok(const T& val)` | Construct a success result (`const&`) |
| `Result::ok(T&& val)` | Construct a success result (move) |
| `Result::err(const Error& e)` | Construct an error result |

**Query:**

| Method | Description |
|:-------|:------------|
| `is_ok()` | `true` if holding a value |
| `is_err()` | `true` if holding an error |

**Accessors** (precondition: must match state):

| Method | Precondition | Returns |
|:-------|:-------------|:--------|
| `value()` | `is_ok()` | `T&` or `const T&` |
| `error()` | `is_err()` | `Error&` or `const Error&` |

**Lifecycle:** Copyable, movable. Destructor calls the appropriate destructor for the active member.

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

Type trait that removes `&` and `&&` from a type. Used internally by `move()`.

### `template<typename T> T&& move(T& t) noexcept`

Cast `t` to an rvalue reference, enabling move semantics. Replacement for `std::move`.

---

## 📁 String View — `str_view.h`

### `class StringView`

A lightweight, non-owning reference to a contiguous sequence of characters.
- No allocations, no copies, no ownership
- All operations are `constexpr` where possible
- Caller must ensure referenced data outlives the `StringView`

**Constants:**

| Member | Value |
|:-------|:------|
| `StringView::npos` | `size_t(-1)` — sentinel for "not found" |

**Constructors:**

| Signature | Description |
|:----------|:------------|
| `StringView()` | Empty view (`data() == nullptr`, `size() == 0`) |
| `StringView(const char* s)` | From null-terminated string. Accepts `nullptr` → empty. |
| `StringView(const char* s, size_t len)` | From pointer + explicit length. Not null-terminated. |
| `StringView(nullptr_t)` | Explicit nullptr → empty view |

**Observers:**

| Method | Description |
|:-------|:------------|
| `data()` | Raw pointer to the character data (not guaranteed null-terminated) |
| `size()`, `length()` | Number of characters |
| `empty()` | `true` if `size() == 0` |
| `operator[](size_t i)` | Character at index `i` (no bounds check, UB if out of range) |

**Comparison:**

| Method | Description |
|:-------|:------------|
| `operator==(StringView, StringView)` | Byte-for-byte equality |
| `operator!=(StringView, StringView)` | Negation of `==` |

**Search:**

| Method | Description |
|:-------|:------------|
| `starts_with(const char* prefix)` | `true` if view starts with `prefix` |
| `find(const char* s, size_t pos = 0)` | First occurrence of substring `s` at or after `pos`. Returns `npos` if not found. |
| `find(char c, size_t pos = 0)` | First occurrence of character `c` at or after `pos`. Returns `npos` if not found. |

**Substring:**

| Method | Description |
|:-------|:------------|
| `substr(size_t offset, size_t count = npos)` | Substring starting at `offset`, at most `count` chars. Clamps to available length. Returns empty view if `offset >= size()`. |

**Utilities:**

| Method | Description |
|:-------|:------------|
| `to_int()` | Parse decimal integer. Returns 0 on format error. Clamps to `INT_MAX`/`INT_MIN` on overflow. |
| `hash()` | FNV-1a hash. Auto-selects 64-bit or 32-bit parameters based on platform. |

```cpp
constexpr xnet::StringView sv("hello world");
constexpr bool ok = sv.starts_with("hello");       // true
constexpr size_t p = sv.find('w');                  // 6
constexpr xnet::StringView sub = sv.substr(0, 5);   // "hello"
constexpr int n = xnet::StringView("42").to_int();  // 42
```

---

## 📁 Buffer — `buffer.h`

### `class Allocator` (abstract)

Memory allocation interface. Implement to provide custom allocators (pool, arena, static).

```cpp
class Allocator {
    virtual void* allocate(size_t size) = 0;
    virtual void deallocate(void* ptr, size_t size) = 0;
};
```

### `class MallocAllocator`

Default `Allocator` implementation using `malloc()`/`free()`.

### `class Buffer`

A resizable byte buffer that owns its storage. Move-only — no implicit copies.

**Constants:**

| Member | Value |
|:-------|:------|
| `Buffer::npos` | `size_t(-1)` — sentinel for "not found" |

**Construction:**

| Signature | Description |
|:----------|:------------|
| `Buffer()` | Empty buffer, uses default `MallocAllocator` |
| `explicit Buffer(Allocator* allocator)` | Empty buffer with custom allocator |
| `Buffer(Buffer&& other)` | Move constructor — transfers ownership |
| `Buffer(const Buffer&)` | **Deleted** — use `clone()` instead |

**Observers:**

| Method | Description |
|:-------|:------------|
| `data()` | Raw pointer to data (`const char*` or `char*`) |
| `mutable_data()` | `char*` mutable pointer |
| `size()` | Number of bytes written |
| `capacity()` | Allocated capacity |
| `empty()` | `true` if `size() == 0` |
| `clear()` | Reset `size()` to 0 (does not release memory) |

**Element access:**

| Method | Description |
|:-------|:------------|
| `operator[](size_t)` | Byte at index (no bounds check) |

**Modifiers:**

| Method | Description |
|:-------|:------------|
| `append(const void* data, size_t len)` | Append raw bytes. Auto-reserves if needed. |
| `append(const Buffer& other)` | Append from another buffer |
| `append(char byte)` | Append single byte |
| `pop_front(size_t n)` | Remove first `n` bytes. Efficient `memmove`-based. |
| `resize(size_t new_size)` | Resize. Grows with zero-initialization, shrinks by truncation. |
| `reserve(size_t new_capacity)` | Pre-allocate capacity. No-op if requested <= current. |
| `swap(Buffer& other)` | Exchange contents with another buffer (O(1), no alloc). |

**Search:**

| Method | Description |
|:-------|:------------|
| `find(const char* needle, size_t needle_len)` | Find first occurrence of `needle`. Returns offset or `npos`. |

**Clone:**

| Method | Description |
|:-------|:------------|
| `clone()` | Deep copy using the same allocator |

```cpp
xnet::Buffer buf;
buf.append("Hello, ", 7);
buf.append("World!", 6);
XNET_ASSERT(buf.size() == 13);
```

---

## 📁 URL — `url.h`

### `class Url`

RFC 3986 URL parser. All components are `StringView` references to the original input string.
No heap allocations during parsing.

**Fields:**

| Field | Type | Description |
|:------|:-----|:------------|
| `scheme` | `StringView` | Protocol scheme (`"http"`, `"https"`, empty if absent) |
| `host` | `StringView` | Hostname or IP (IPv6 without brackets, e.g. `"::1"`) |
| `path` | `StringView` | Path component (`"/index.html"`, empty if absent) |
| `query` | `StringView` | Query string (`"q=hello&n=1"`, empty if absent) |
| `fragment` | `StringView` | Fragment identifier (`"section-2"`, empty if absent) |
| `username` | `StringView` | Userinfo username (empty if absent) |
| `password` | `StringView` | Userinfo password (empty if absent) |
| `port` | `int` | Port number. `0` means "not specified". |

**Static methods:**

| Method | Description |
|:-------|:------------|
| `parse(const char* str, size_t len)` | Parse URL. Returns `Result<Url>`. Accepts absolute (`http://...`), scheme-relative (`//host/path`), path-relative (`/path`), query-only (`?q=1`), fragment-only (`#frag`). |
| `encode(const StringView& input, char* output, size_t output_size, size_t* written)` | Percent-encode. Unreserved chars (`A-Z a-z 0-9 - . _ ~`) pass through; others become `%XX`. |
| `decode(const StringView& input, char* output, size_t output_size, size_t* written)` | Percent-decode. `%XX` sequences decoded; `+` becomes space. |

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

| Enumerator | Wire format |
|:-----------|:------------|
| `GET` | `"GET"` |
| `POST` | `"POST"` |
| `PUT` | `"PUT"` |
| `DELETE` | `"DELETE"` |
| `HEAD` | `"HEAD"` |
| `PATCH` | `"PATCH"` |

`to_string(Method)` returns the wire format string.

### `enum class Version : uint8_t`

| Enumerator | Wire format |
|:-----------|:------------|
| `HTTP_1_0` | `"HTTP/1.0"` |
| `HTTP_1_1` | `"HTTP/1.1"` |
| `HTTP_2_0` | `"HTTP/2.0"` |

`to_string(Version)` returns the wire format string.

### `struct Header`

| Field | Type | Description |
|:------|:-----|:------------|
| `name` | `StringView` | Header name (e.g. `"Content-Type"`) |
| `value` | `StringView` | Header value (e.g. `"application/json"`) |

### `struct HttpRequest`

Low-level HTTP request representation. Usable directly, or through the `Request` builder.

| Field | Type | Description |
|:------|:-----|:------------|
| `method` | `Method` | HTTP method |
| `url` | `StringView` | Request path/URL |
| `version` | `Version` | HTTP version (default: 1.1) |
| `headers` | `Header[kMaxHeaders]` | Header array (max 64) |
| `num_headers` | `size_t` | Number of valid headers |
| `body` | `Buffer*` | Non-owning pointer to body (may be `nullptr`) |

| Method | Description |
|:-------|:------------|
| `serialize(Buffer& out)` | Serialize to wire format (`METHOD path HTTP/1.1\r\n...`) |

### `struct HttpResponse`

Parsed HTTP response. `parse()` stores header `StringView` references in the provided `header_storage` buffer.

| Field | Type | Description |
|:------|:-----|:------------|
| `version` | `Version` | HTTP version |
| `status_code` | `int` | Status code (200, 404, etc.) |
| `reason` | `StringView` | Reason phrase (e.g. "OK", "Not Found") |
| `headers` | `Header[kMaxHeaders]` | Parsed headers |
| `num_headers` | `size_t` | Number of parsed headers |
| `body` | `Buffer` | Response body (owned) |

| Method | Description |
|:-------|:------------|
| `static parse(const char* data, size_t len, Buffer& header_storage)` | Parse HTTP response. Returns `Result<HttpResponse>`. |

---

## 📁 Socket — `socket.h`

### `struct SocketError : Error`

Convenience factory for socket-related errors.

```cpp
xnet::SocketError::connection_refused("port 80 closed");
xnet::SocketError::timeout("connect timed out");
xnet::SocketError::dns_failure("host not found");
```

### `class Socket` (abstract)

TCP socket I/O interface.

| Method | Description |
|:-------|:------------|
| `connect(const char* host, int port, int timeout_ms)` | Connect to `host:port`. `timeout_ms <= 0` = infinite. |
| `send(const void* data, size_t len)` | Send bytes. Returns actual bytes sent or error. |
| `recv(void* buf, size_t max_len)` | Receive bytes. Returns 0 on EOF (peer closed). |
| `close()` | Close socket. Idempotent (safe to call multiple times). |

### `class SocketFactory`

Static factory for creating platform-specific `Socket` instances.

| Method | Description |
|:-------|:------------|
| `create(const char* host, int port)` | Create a platform TCP socket (unconnected). Returns `Result<Socket*>`. |
| `destroy(Socket* s)` | Destroy a socket. Auto-closes. `nullptr`-safe. |

---

## 📁 Request / Response — `request.h`

### `struct Response`

High-level HTTP response with owned storage.

| Method | Description |
|:-------|:------------|
| `status_code()` | HTTP status code (`int`) |
| `body()` | Response body (`const char*` or `nullptr`). Lifetime tied to `Response`. |
| `body_size()` | Response body size (`size_t`) |
| `version()` | HTTP version |
| `header(const char* name)` | Look up header by name (case-insensitive). Returns `const char*` or `nullptr`. |
| `num_headers()` | Number of headers |
| `header_at(size_t i)` | Header at index `i` |

### `class Request`

Builder-pattern HTTP request. Default timeout: 30 seconds.

**Builder methods** (all return `Request&` for chaining):

| Method | Description |
|:-------|:------------|
| `url(const char* url)` | Set request URL. Copied internally. |
| `method(Method m)` | Set HTTP method |
| `header(const char* name, const char* value)` | Add header. Name and value copied internally. |
| `body(const char* data, size_t len)` | Set request body. Copied internally. |
| `timeout(int ms)` | Set timeout in ms. `<= 0` = no timeout. |

**Execution:**

| Method | Description |
|:-------|:------------|
| `perform()` | Execute request. Parse URL → connect → send → receive → parse response. Returns `Result<Response>`. |

### Free Functions

```cpp
xnet::get("http://example.com/");
xnet::post("http://example.com/", json, json_len);
xnet::put("http://example.com/", body, body_len);
xnet::del("http://example.com/");
```

### Header Lookup Semantics

- Case-insensitive matching per RFC 7230
- `nullptr` returned for missing headers
- Returned `const char*` is null-terminated (parser appends `\0` to storage)
- Lifetime bound to the `Response` object

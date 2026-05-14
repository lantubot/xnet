# XNet Documentation

> Lightweight, STL-free C++20 networking library for embedded systems.

## 📚 Contents

| Document | Description |
|:---------|:------------|
| [API Reference](api.md) | Complete API documentation for all modules |
| [Usage Guide](usage.md) | Examples, patterns, tips & tricks |
| [Build Guide](build.md) | Build options, cross-compilation, CI |
| [Platform Notes](platforms.md) | Linux / Windows / ESP32 specifics |

## 🏗️ Architecture

```
┌──────────────────────────────────────────────────┐
│              xnet::Request (builder)              │
│              xnet::Response (result)              │
├──────────────────────────────────────────────────┤
│   xnet::HttpRequest    │   xnet::HttpResponse     │
│   xnet::Method         │   xnet::Version          │
│   xnet::Header                                   │
├──────────────────────────────────────────────────┤
│   xnet::Url (RFC 3986) │   xnet::Socket (abstract)│
├──────────────────────┬─┴─────────────────────────┤
│ xnet::Buffer         │   xnet::SocketFactory     │
│ xnet::StringView     │   ├─ LinuxTcpSocket       │
│ xnet::Status         │   ├─ WinTcpSocket         │
│ xnet::Error          │   └─ Esp32TcpSocket       │
│ xnet::Result<T>      │                           │
└──────────────────────┴───────────────────────────┘
```

## 🎯 Design Principles

1. **Zero STL** — no `std::vector`, `std::string`, exceptions, or RTTI
2. **C++20 constexpr** — compile-time string ops, URL validation
3. **Fixed-width types** — `uint32_t`/`int32_t` everywhere; no 64-bit emulation on ESP32
4. **Non-owning by default** — `StringView`-based APIs, caller manages lifetimes
5. **Move semantics** — `Buffer` moves, never copies implicitly
6. **Error as values** — `Result<T>` instead of exceptions
7. **Builder pattern** — fluent `Request` API (`url().method().header().body()`)

## 🚦 Quick Reference

```cpp
// GET one-liner
xnet::Result<xnet::Response> r = xnet::get("http://example.com/");

// Full builder
xnet::Result<xnet::Response> r = xnet::Request()
    .url("http://api.example.com/data")
    .method(xnet::Method::POST)
    .header("Content-Type", "application/json")
    .body(json_str, json_len)
    .timeout(5000)
    .perform();
```

See the [Usage Guide](usage.md) for complete examples.

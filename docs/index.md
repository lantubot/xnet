# XNet 文档

> 轻量级、无 STL 的 C++20 嵌入式系统网络库

## 📚 目录

| 文档 | 说明 |
|:-----|:-----|
| [API 参考](api.md) | 所有模块的完整 API 文档 |
| [使用指南](usage.md) | 示例、模式、技巧和提示 |
| [构建指南](build.md) | 构建选项、交叉编译、持续集成 |
| [平台说明](platforms.md) | Linux / Windows / ESP32 平台特性 |

## 🏗️ 架构

```
┌──────────────────────────────────────────────────┐
│              xnet::Request（构建器）               │
│              xnet::Response（结果）                │
├──────────────────────────────────────────────────┤
│   xnet::HttpRequest    │   xnet::HttpResponse     │
│   xnet::Method         │   xnet::Version          │
│   xnet::Header                                   │
├──────────────────────────────────────────────────┤
│   xnet::Url (RFC 3986) │   xnet::Socket（抽象）    │
├──────────────────────┬─┴─────────────────────────┤
│ xnet::Buffer         │   xnet::SocketFactory     │
│ xnet::StringView     │   ├─ LinuxTcpSocket       │
│ xnet::Status         │   ├─ WinTcpSocket         │
│ xnet::Error          │   └─ Esp32TcpSocket       │
│ xnet::Result<T>      │                           │
└──────────────────────┴───────────────────────────┘
```

## 🎯 设计原则

1. **零 STL** — 不使用 `std::vector`、`std::string`、异常或 RTTI
2. **C++20 constexpr** — 编译时字符串操作、URL 校验
3. **固定宽度类型** — 统一使用 `uint32_t`/`int32_t`；ESP32 上无需 64 位仿真
4. **默认非拥有语义** — 基于 `StringView` 的 API，调用方管理生命周期
5. **移动语义** — `Buffer` 移动而非隐式拷贝
6. **错误即值** — 使用 `Result<T>` 替代异常
7. **构建器模式** — 流畅的 `Request` API（`url().method().header().body()`）

## 🚦 快速参考

```cpp
// GET 一键调用
xnet::Result<xnet::Response> r = xnet::get("http://example.com/");

// 完整构建器
xnet::Result<xnet::Response> r = xnet::Request()
    .url("http://api.example.com/data")
    .method(xnet::Method::POST)
    .header("Content-Type", "application/json")
    .body(json_str, json_len)
    .timeout(5000)
    .perform();
```

完整示例请参阅[使用指南](usage.md)。

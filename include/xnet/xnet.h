#pragma once

// ============================================================================
// xnet — lightweight, STL-free C++20 networking library
// ============================================================================
//
// xnet is a minimal, header-based networking library for C++20 that operates
// without any C++ Standard Library dependencies. It provides:
//
//   - error.h     — Status codes and error handling primitives
//   - string_view.h — A lightweight std::string_view replacement
//   - buffer.h    — Contiguous byte buffer (growable / fixed-capacity)
//   - url.h       — URL parsing and component access
//   - http.h      — HTTP request / response parsing and building
//   - socket.h    — Platform socket abstraction (connect, send, recv)
//   - request.h   — High-level request / response model
//
// All types live under the `xnet::` namespace. Include this single header to
// pull in the entire public API.
// ============================================================================

#include "xnet/error.h"
#include "xnet/string_view.h"
#include "xnet/buffer.h"
#include "xnet/url.h"
#include "xnet/http.h"
#include "xnet/socket.h"
#include "xnet/request.h"

#pragma once

/** @file xnet.h
 * @brief Lightweight, STL-free C++20 networking library.
 *
 * xnet is a minimal, header-based networking library for C++20 that operates
 * without any C++ Standard Library dependencies. It bundles the following
 * modules, each in its own header under the `xnet/` directory:
 *
 * - @b error.h      — Status codes and error handling primitives
 * - @b str_view.h   — A lightweight std::string_view replacement
 * - @b buffer.h     — Contiguous byte buffer (growable / fixed-capacity)
 * - @b url.h        — URL parsing and component access
 * - @b http.h       — HTTP request / response parsing and building
 * - @b socket.h     — Platform socket abstraction (connect, send, recv)
 * - @b request.h    — High-level request / response model
 *
 * All types live under the @c xnet:: namespace. Including this single header
 * pulls in the entire public API.
 */

#include "xnet/buffer.h"
#include "xnet/error.h"
#include "xnet/http.h"
#include "xnet/request.h"
#include "xnet/socket.h"
#include "xnet/str_view.h"
#include "xnet/url.h"

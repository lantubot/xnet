// Copyright (c) 2026 XNet Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#ifndef XNET_HTTP_H_
#define XNET_HTTP_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "xnet/buffer.h"
#include "xnet/error.h"
#include "xnet/string_view.h"

namespace xnet {

// ============================================================================
// Method — HTTP request methods
// ============================================================================
enum class Method : uint8_t {
  GET = 0,
  POST,
  PUT,
  DELETE,
  HEAD,
  PATCH,
};

// Returns the wire-format string for |m| (e.g. GET → "GET").
inline const char* to_string(Method m) {
  switch (m) {
    case Method::GET:    return "GET";
    case Method::POST:   return "POST";
    case Method::PUT:    return "PUT";
    case Method::DELETE: return "DELETE";
    case Method::HEAD:   return "HEAD";
    case Method::PATCH:  return "PATCH";
    default:             return "UNKNOWN";
  }
}

// ============================================================================
// Version — HTTP protocol versions
// ============================================================================
enum class Version : uint8_t {
  HTTP_1_0 = 0,
  HTTP_1_1,
  HTTP_2_0,
};

// Returns the wire-format string for |v| (e.g. HTTP_1_1 → "HTTP/1.1").
inline const char* to_string(Version v) {
  switch (v) {
    case Version::HTTP_1_0: return "HTTP/1.0";
    case Version::HTTP_1_1: return "HTTP/1.1";
    case Version::HTTP_2_0: return "HTTP/2.0";
    default:                return "HTTP/1.1";
  }
}

// ============================================================================
// Header — a single name/value pair.
//
// Both |name| and |value| are StringViews.  For parsed responses they point
// into the |header_storage| Buffer passed to HttpResponse::parse().
// ============================================================================
struct Header {
  StringView name;
  StringView value;
};

// ============================================================================
// HttpRequest — an outgoing HTTP request.
//
// Fields are public for convenient construction.  The caller owns the memory
// backing |url| and all Header name/value strings.  The optional |body|
// pointer must outlive any call to serialize().
// ============================================================================
struct HttpRequest {
  Method                     method;
  StringView                 url;
  Version                    version;

  // Maximum number of headers that can be stored inline.
  static constexpr size_t    kMaxHeaders = 64;

  Header                     headers[kMaxHeaders];
  size_t                     num_headers;

  // Non-owning pointer to the request body.  May be nullptr.
  Buffer*                    body;

  // Serialize this request into |out| in HTTP/1.x wire format:
  //   METHOD /path HTTP/1.1\r\n
  //   Header: value\r\n
  //   \r\n
  //   body...
  Status serialize(Buffer& out) const;
};

// ============================================================================
// HttpResponse — a parsed HTTP response.
//
// After a successful call to parse(), |headers| entries contain StringViews
// pointing into the supplied |header_storage| buffer.  The response body is
// owned by this struct.
// ============================================================================
struct HttpResponse {
  Version                    version;
  int                        status_code;
  StringView                 reason;

  static constexpr size_t    kMaxHeaders = 64;

  Header                     headers[kMaxHeaders];
  size_t                     num_headers;

  Buffer                     body;

  // Parse an HTTP response message from |data| (|len| bytes).
  //
  // |header_storage| is used as the backing store for the StringView members
  // of every Header entry.  The caller must ensure that |header_storage|
  // outlives this HttpResponse object.
  //
  // Returns the parsed response on success, or an Error describing the
  // parse failure.
  static Result<HttpResponse> parse(const char* data, size_t len,
                                    Buffer& header_storage);
};

// ============================================================================
// HttpRequest::serialize
// ============================================================================
inline Status HttpRequest::serialize(Buffer& out) const {
  // --- Request line ---------------------------------------------------------
  out.append(to_string(method), std::strlen(to_string(method)));
  out.append(' ');

  out.append(url.data(), url.size());
  out.append(' ');

  out.append(to_string(version), std::strlen(to_string(version)));
  out.append('\r');
  out.append('\n');

  // --- Headers --------------------------------------------------------------
  for (size_t i = 0; i < num_headers; ++i) {
    out.append(headers[i].name.data(), headers[i].name.size());
    out.append(':');
    out.append(' ');
    out.append(headers[i].value.data(), headers[i].value.size());
    out.append('\r');
    out.append('\n');
  }

  // --- Empty line (end of headers) -----------------------------------------
  out.append('\r');
  out.append('\n');

  // --- Body -----------------------------------------------------------------
  if (body != nullptr && body->size() > 0) {
    out.append(body->data(), body->size());
  }

  return Status::OK;
}

// ============================================================================
// HttpResponse::parse
// ============================================================================
inline Result<HttpResponse> HttpResponse::parse(const char* data, size_t len,
                                                 Buffer& header_storage) {
  // --- Initialise a default response ----------------------------------------
  HttpResponse resp;
  resp.version      = Version::HTTP_1_1;
  resp.status_code  = 0;
  resp.num_headers  = 0;

  if (data == nullptr || len == 0) {
    return Result<HttpResponse>::err(
        Error(Status::INVALID_ARGUMENT, "empty response data"));
  }

  size_t pos = 0;

  // ==========================================================================
  // 1. Status line: "HTTP/1.1 200 OK\r\n"
  // ==========================================================================

  // Find the first space — everything before it is the protocol version.
  size_t space1 = pos;
  while (space1 < len && data[space1] != ' ') {
    ++space1;
  }
  if (space1 == len || space1 == pos) {
    return Result<HttpResponse>::err(
        Error(Status::PROTOCOL_ERROR, "cannot find HTTP version in status line"));
  }

  // Determine the version.
  StringView ver(data + pos, space1 - pos);
  if (ver == StringView("HTTP/1.0")) {
    resp.version = Version::HTTP_1_0;
  } else if (ver == StringView("HTTP/1.1")) {
    resp.version = Version::HTTP_1_1;
  } else if (ver == StringView("HTTP/2.0")) {
    resp.version = Version::HTTP_2_0;
  } else {
    return Result<HttpResponse>::err(
        Error(Status::UNSUPPORTED_PROTOCOL, "unsupported HTTP version"));
  }

  pos = space1 + 1;

  // Find the second space — the status code sits between the two spaces.
  size_t space2 = pos;
  while (space2 < len && data[space2] != ' ') {
    ++space2;
  }
  if (space2 == len || space2 == pos) {
    return Result<HttpResponse>::err(
        Error(Status::PROTOCOL_ERROR, "cannot find status code"));
  }

  // Parse decimal status code.
  int code = 0;
  for (size_t i = pos; i < space2; ++i) {
    if (data[i] < '0' || data[i] > '9') {
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "non-digit in status code"));
    }
    code = code * 10 + static_cast<int>(data[i] - '0');
  }
  resp.status_code = code;

  pos = space2 + 1;

  // Find the \r\n that terminates the status line.
  size_t crlf = pos;
  while (crlf + 1 < len && !(data[crlf] == '\r' && data[crlf + 1] == '\n')) {
    ++crlf;
  }
  if (crlf + 1 >= len) {
    return Result<HttpResponse>::err(
        Error(Status::PROTOCOL_ERROR, "unterminated status line"));
  }

  // Store the reason phrase in header_storage.
  {
    size_t rlen = crlf - pos;
    size_t roff = header_storage.size();
    header_storage.append(data + pos, rlen);
    header_storage.append('\0');
    resp.reason = StringView(header_storage.data() + roff, rlen);
  }

  pos = crlf + 2;

  // ==========================================================================
  // 2. Header fields
  // ==========================================================================
  while (pos + 1 < len) {
    // Empty line signals the end of headers.
    if (data[pos] == '\r' && data[pos + 1] == '\n') {
      pos += 2;
      break;
    }

    if (resp.num_headers >= kMaxHeaders) {
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "exceeded maximum number of headers"));
    }

    // Locate the colon separating name from value.
    size_t colon = pos;
    while (colon < len && data[colon] != ':') {
      ++colon;
    }
    if (colon == len || colon == pos) {
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "malformed header: missing colon"));
    }

    // Header name (trimmed to colon position, no trailing space stripping).
    {
      size_t noff = header_storage.size();
      header_storage.append(data + pos, colon - pos);
      header_storage.append('\0');
      resp.headers[resp.num_headers].name =
          StringView(header_storage.data() + noff, colon - pos);
    }

    // Skip past the colon and any leading whitespace on the value.
    size_t val_start = colon + 1;
    while (val_start < len && data[val_start] == ' ') {
      ++val_start;
    }

    // Find the \r\n terminating this header line.
    size_t eol = val_start;
    while (eol + 1 < len && !(data[eol] == '\r' && data[eol + 1] == '\n')) {
      ++eol;
    }
    if (eol + 1 >= len) {
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "unterminated header line"));
    }

    // Header value.
    {
      size_t voff = header_storage.size();
      header_storage.append(data + val_start, eol - val_start);
      header_storage.append('\0');
      resp.headers[resp.num_headers].value =
          StringView(header_storage.data() + voff, eol - val_start);
    }

    ++resp.num_headers;
    pos = eol + 2;
  }

  // ==========================================================================
  // 3. Body — everything remaining after the header terminator
  // ==========================================================================
  if (pos < len) {
    resp.body.append(data + pos, len - pos);
  }

  return Result<HttpResponse>::ok(move(resp));
}

}  // namespace xnet

#endif  // XNET_HTTP_H_

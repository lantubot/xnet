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

#ifndef XNET_REQUEST_H_
#define XNET_REQUEST_H_

#include "xnet/buffer.h"
#include "xnet/error.h"
#include "xnet/http.h"
#include "xnet/socket.h"
#include "xnet/string_view.h"
#include "xnet/url.h"

#include <cstddef>
#include <cstring>

namespace xnet {

// ============================================================================
// Response — holds the result of a completed HTTP request.
//
// Owns a Buffer that backs both the parsed header storage and the response
// body.  Provides high-level accessors for status code, body content, and
// header lookup.
// ============================================================================
struct Response {
  // --------------------------------------------------------------------------
  // Construction
  // --------------------------------------------------------------------------
  Response() : status_code_(0), version_(Version::HTTP_1_1), num_headers_(0) {
    for (size_t i = 0; i < HttpResponse::kMaxHeaders; ++i) {
      headers_[i].name = StringView();
      headers_[i].value = StringView();
    }
  }

  // --------------------------------------------------------------------------
  // Accessors
  // --------------------------------------------------------------------------

  // Returns the HTTP status code (e.g. 200, 404, 500).
  int status_code() const { return status_code_; }

  // Returns a pointer to the response body data.  May be nullptr if the
  // response has no body.  The returned pointer is valid for the lifetime
  // of this Response object.
  const char* body() const {
    return body_.empty() ? nullptr : body_.data();
  }

  // Returns the size of the response body in bytes.
  size_t body_size() const { return body_.size(); }

  // Returns the HTTP version of the response.
  Version version() const { return version_; }

  // Finds a header by |name| (case-insensitive comparison).
  // Returns the header value, or nullptr if no matching header is found.
  // The returned pointer is valid for the lifetime of this Response object.
  const char* header(const char* name) const {
    if (name == nullptr) return nullptr;

    size_t name_len = std::strlen(name);
    for (size_t i = 0; i < num_headers_; ++i) {
      if (headers_[i].name.size() == name_len &&
          CaselessCompare(headers_[i].name.data(), name, name_len)) {
        // Return value as a null-terminated string.  During parsing, headers
        // are stored with a trailing '\0' in |storage_|, so the StringView
        // data is already null-terminated.
        return headers_[i].value.data();
      }
    }
    return nullptr;
  }

  // Returns the number of headers in the response.
  size_t num_headers() const { return num_headers_; }

  // Returns the header at index |i|.  Behaviour is undefined if |i| >=
  // num_headers().
  const Header& header_at(size_t i) const {
    return headers_[i];
  }

 private:
  friend class Request;  // Request builds Responses.

  int       status_code_;
  Version   version_;
  Header    headers_[HttpResponse::kMaxHeaders];
  size_t    num_headers_;
  Buffer    storage_;   // Backing store for header name/value StringViews.
  Buffer    body_;      // Owned response body.

  // --------------------------------------------------------------------------
  // Case-insensitive comparison of two ASCII strings of equal length.
  // --------------------------------------------------------------------------
  static bool CaselessCompare(const char* a, const char* b, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      char ca = a[i];
      char cb = b[i];
      if (ca == cb) continue;
      // Fold [A-Z] to [a-z].
      if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca + 32);
      if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb + 32);
      if (ca != cb) return false;
    }
    return true;
  }
};

// ============================================================================
// Request — builder-pattern HTTP request.
//
// Example usage:
//   Result<Response> r = xnet::Request()
//       .url("https://api.example.com/data")
//       .method(Method::POST)
//       .header("Content-Type", "application/json")
//       .body("{\"key\":\"value\"}", 15)
//       .timeout(5000)
//       .perform();
// ============================================================================
class Request {
 public:
  // Default timeout in milliseconds.
  static constexpr int kDefaultTimeoutMs = 30000;

  // --------------------------------------------------------------------------
  // Constructor
  // --------------------------------------------------------------------------
  Request()
      : method_(Method::GET),
        timeout_ms_(kDefaultTimeoutMs),
        num_headers_(0) {}

  // --------------------------------------------------------------------------
  // Builder methods (fluent)
  // --------------------------------------------------------------------------

  // Sets the request URL.  The string is copied internally so it does not
  // need to outlive the builder.
  Request& url(const char* url) {
    if (url != nullptr) {
      url_.clear();
      url_.append(url, std::strlen(url));
      url_.append('\0');  // null-terminate for convenience
    }
    return *this;
  }

  // Sets the HTTP method.
  Request& method(Method m) {
    method_ = m;
    return *this;
  }

  // Adds a header.  Both |name| and |value| are copied internally.
  // Returns a reference to this Request for chaining.
  Request& header(const char* name, const char* value) {
    if (name == nullptr || value == nullptr) return *this;
    if (num_headers_ >= HttpRequest::kMaxHeaders) return *this;

    size_t name_len = std::strlen(name);
    size_t val_len = std::strlen(value);

    // Store the name in header_storage_.
    size_t name_off = header_storage_.size();
    header_storage_.append(name, name_len);
    header_storage_.append('\0');

    // Store the value in header_storage_.
    size_t val_off = header_storage_.size();
    header_storage_.append(value, val_len);
    header_storage_.append('\0');

    headers_[num_headers_].name =
        StringView(header_storage_.data() + name_off, name_len);
    headers_[num_headers_].value =
        StringView(header_storage_.data() + val_off, val_len);
    ++num_headers_;

    return *this;
  }

  // Sets the request body.  |data| is copied internally so it does not need
  // to outlive the builder.
  Request& body(const char* data, size_t len) {
    body_.clear();
    if (data != nullptr && len > 0) {
      body_.append(data, len);
    }
    return *this;
  }

  // Sets the timeout in milliseconds.  Values <= 0 mean no timeout.
  Request& timeout(int ms) {
    timeout_ms_ = ms;
    return *this;
  }

  // --------------------------------------------------------------------------
  // Perform — execute the HTTP request.
  //
  // Parses the URL, connects to the remote server, sends the HTTP request,
  // receives the response, and returns a Result<Response>.
  //
  // On failure returns an Error describing the problem (DNS failure,
  // connection refused, timeout, protocol error, etc.).
  // --------------------------------------------------------------------------
  Result<Response> perform() {
    // --- 1. Parse the URL ---------------------------------------------------
    if (url_.empty()) {
      return Result<Response>::err(
          Error(Status::INVALID_ARGUMENT, "request URL is not set"));
    }

    Result<Url> url_result = Url::parse(url_.data(), url_.size() - 1);  // -1 for \0
    if (url_result.is_err()) {
      return Result<Response>::err(url_result.error());
    }

    const Url& target = url_result.value();

    // --- 2. Determine host and port -----------------------------------------
    if (target.host.empty()) {
      return Result<Response>::err(
          Error(Status::INVALID_ARGUMENT, "URL has no host"));
    }

    // Determine default port based on scheme.
    int port = target.port;
    if (port == 0) {
      if (target.scheme == StringView("https")) {
        port = 443;
      } else {
        port = 80;  // default for http and everything else
      }
    }

    // Build a null-terminated host string.
    host_.clear();
    host_.append(target.host.data(), target.host.size());
    host_.append('\0');

    // Build the request path (default to "/" if empty).
    path_.clear();
    if (!target.path.empty()) {
      path_.append(target.path.data(), target.path.size());
    } else {
      char slash = '/';
      path_.append(&slash, 1);
    }
    if (!target.query.empty()) {
      path_.append('?');
      path_.append(target.query.data(), target.query.size());
    }
    // Add null terminator for StringView construction below.
    path_.append('\0');

    // --- 3. Create and connect socket ---------------------------------------
    Result<Socket*> socket_result = SocketFactory::create(host_.data(), port);
    if (socket_result.is_err()) {
      return Result<Response>::err(socket_result.error());
    }
    Socket* sock = socket_result.value();

    // Connect with timeout.
    Status connect_status = sock->connect(host_.data(), port, timeout_ms_);
    if (connect_status != Status::OK) {
      SocketFactory::destroy(sock);
      return Result<Response>::err(
          Error(connect_status, "failed to connect"));
    }

    // --- 4. Build the HTTP request ------------------------------------------
    HttpRequest req;
    req.method = method_;
    req.url = StringView(path_.data(), path_.size() - 1);  // exclude \0
    req.version = Version::HTTP_1_1;
    req.num_headers = 0;
    req.body = nullptr;

    // Copy headers from the builder.
    for (size_t i = 0; i < num_headers_; ++i) {
      req.headers[req.num_headers++] = headers_[i];
    }

    // Add Host header if not already present.
    if (!HasHeader("Host")) {
      req.headers[req.num_headers].name = StringView("Host", 4);
      req.headers[req.num_headers].value =
          StringView(host_.data(), host_.size() - 1);
      ++req.num_headers;
    }

    // Add Content-Length header for non-empty body.
    if (body_.size() > 0 && !HasHeader("Content-Length")) {
      // We need a small buffer to format the content-length value as decimal.
      // Use a stack buffer large enough for any 64-bit integer.
      char cl_buf[32];
      size_t cl_len = FormatDecimal(cl_buf, sizeof(cl_buf),
                                    static_cast<unsigned long long>(body_.size()));
      // Store the formatted string in header_storage_ so it lives long enough.
      size_t cl_off = header_storage_.size();
      header_storage_.append(cl_buf, cl_len);
      header_storage_.append('\0');
      req.headers[req.num_headers].name = StringView("Content-Length", 14);
      req.headers[req.num_headers].value =
          StringView(header_storage_.data() + cl_off, cl_len);
      ++req.num_headers;
    }

    // Set body if present.
    Buffer* body_ptr = nullptr;
    if (body_.size() > 0) {
      body_ptr = &body_;
    }
    req.body = body_ptr;

    // --- 5. Serialize the request -------------------------------------------
    Buffer wire_buffer;
    Status ser_status = req.serialize(wire_buffer);
    if (ser_status != Status::OK) {
      SocketFactory::destroy(sock);
      return Result<Response>::err(
          Error(ser_status, "failed to serialize request"));
    }

    // --- 6. Send the request ------------------------------------------------
    Result<size_t> send_result = sock->send(wire_buffer.data(), wire_buffer.size());
    if (send_result.is_err()) {
      SocketFactory::destroy(sock);
      return Result<Response>::err(send_result.error());
    }

    // Keep sending until all data is transmitted.
    size_t total_sent = send_result.value();
    while (total_sent < wire_buffer.size()) {
      send_result = sock->send(wire_buffer.data() + total_sent,
                               wire_buffer.size() - total_sent);
      if (send_result.is_err()) {
        SocketFactory::destroy(sock);
        return Result<Response>::err(send_result.error());
      }
      total_sent += send_result.value();
    }

    // --- 7. Receive the response --------------------------------------------
    Buffer recv_buf;

    // Accumulate data until the remote end closes the connection or an error
    // occurs.  A production implementation would parse Content-Length or
    // Transfer-Encoding: chunked to determine the exact response size, but
    // for simplicity we read until EOF (common for HTTP/1.0 and many
    // HTTP/1.1 servers with Connection: close).
    const size_t kRecvChunkSize = 4096;
    char chunk[kRecvChunkSize];

    for (;;) {
      Result<size_t> nread = sock->recv(chunk, kRecvChunkSize);
      if (nread.is_err()) {
        SocketFactory::destroy(sock);
        return Result<Response>::err(nread.error());
      }
      if (nread.value() == 0) {
        // Clean EOF from the server — all data has been received.
        break;
      }
      recv_buf.append(chunk, nread.value());
    }

    // Close the connection (we've received the response).
    sock->close();
    SocketFactory::destroy(sock);

    // --- 8. Parse the response ----------------------------------------------
    // HttpResponse::parse() needs a separate header storage buffer.
    Buffer header_storage;
    Result<HttpResponse> parse_result =
        HttpResponse::parse(recv_buf.data(), recv_buf.size(), header_storage);
    if (parse_result.is_err()) {
      return Result<Response>::err(parse_result.error());
    }

    // --- 9. Build the Response ----------------------------------------------
    HttpResponse& http_result = parse_result.value();
    Response resp;
    resp.status_code_ = http_result.status_code;
    resp.version_ = http_result.version;
    resp.num_headers_ = http_result.num_headers;

    // Move the header storage so StringViews remain valid.
    resp.storage_ = static_cast<Buffer&&>(header_storage);

    // Copy header StringViews — they now point into resp.storage_.
    for (size_t i = 0; i < http_result.num_headers; ++i) {
      resp.headers_[i] = http_result.headers[i];
    }

    // Move the response body.
    resp.body_ = static_cast<Buffer&&>(http_result.body);

    return Result<Response>::ok(static_cast<Response&&>(resp));
  }

 private:
  // --------------------------------------------------------------------------
  // Member data
  // --------------------------------------------------------------------------
  Method    method_;
  Buffer    url_;              // Null-terminated URL string.
  Buffer    host_;             // Null-terminated host string (for Host header).
  Buffer    path_;             // Null-terminated request path.
  int       timeout_ms_;
  Header    headers_[HttpRequest::kMaxHeaders];
  size_t    num_headers_;
  Buffer    header_storage_;   // Storage for builder header name/value strings.
  Buffer    body_;             // Request body data.

  // --------------------------------------------------------------------------
  // Check if a header with the given name already exists (case-insensitive).
  // --------------------------------------------------------------------------
  bool HasHeader(const char* name) const {
    if (name == nullptr) return false;
    size_t name_len = std::strlen(name);
    for (size_t i = 0; i < num_headers_; ++i) {
      if (headers_[i].name.size() == name_len &&
          CaselessCompare(headers_[i].name.data(), name, name_len)) {
        return true;
      }
    }
    return false;
  }

  // --------------------------------------------------------------------------
  // Case-insensitive comparison (ASCII).
  // --------------------------------------------------------------------------
  static bool CaselessCompare(const char* a, const char* b, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      char ca = a[i];
      char cb = b[i];
      if (ca == cb) continue;
      if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca + 32);
      if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb + 32);
      if (ca != cb) return false;
    }
    return true;
  }

  // --------------------------------------------------------------------------
  // Format an unsigned integer as decimal into |buf|.
  // Returns the number of characters written (excluding null terminator).
  // --------------------------------------------------------------------------
  static size_t FormatDecimal(char* buf, size_t buf_size,
                              unsigned long long val) {
    if (buf == nullptr || buf_size == 0) return 0;

    // Write digits in reverse order.
    char tmp[32];
    size_t idx = 0;
    if (val == 0) {
      tmp[idx++] = '0';
    } else {
      while (val > 0) {
        tmp[idx++] = static_cast<char>('0' + (val % 10));
        val /= 10;
      }
    }

    // Reverse into output buffer.
    size_t written = idx;
    if (written > buf_size) written = buf_size;
    for (size_t i = 0; i < written; ++i) {
      buf[i] = tmp[idx - 1 - i];
    }
    return written;
  }
};

// ============================================================================
// Free functions — convenience wrappers for common HTTP methods.
// ============================================================================

// Issues an HTTP GET request to |url|.
inline Result<Response> get(const char* url) {
  return Request()
      .url(url)
      .method(Method::GET)
      .perform();
}

// Issues an HTTP POST request to |url| with the given |body|.
inline Result<Response> post(const char* url, const char* body,
                             size_t body_len) {
  return Request()
      .url(url)
      .method(Method::POST)
      .body(body, body_len)
      .perform();
}

// Issues an HTTP PUT request to |url| with the given |body|.
inline Result<Response> put(const char* url, const char* body,
                            size_t body_len) {
  return Request()
      .url(url)
      .method(Method::PUT)
      .body(body, body_len)
      .perform();
}

// Issues an HTTP DELETE request to |url|.
inline Result<Response> del(const char* url) {
  return Request()
      .url(url)
      .method(Method::DELETE)
      .perform();
}

}  // namespace xnet

#endif  // XNET_REQUEST_H_

/** @file src/request.cc
 * @brief Implementation of the HTTP Request builder — URL parsing, socket
 *        I/O, request serialization, and response parsing.
 *
 * Implements the full lifecycle of a single HTTP request:
 *   1. URL parsing (via Url::parse)
 *   2. Socket creation and TCP connect
 *   3. HTTP request serialization (via HttpRequest::serialize)
 *   4. Socket send with retry loop
 *   5. Socket receive with chunked buffering
 *   6. HTTP response parsing (via HttpResponse::parse)
 *   7. Populating the Response result
 */

#include "xnet/request.h"

namespace xnet {

Result<Response> Request::perform() {
  // ---- Step 1: Validate that a URL has been set ---------------------------
  if (url_.empty())
    return Result<Response>::err(
        Error(Status::INVALID_ARGUMENT, "request URL is not set"));

  // ---- Step 2: Parse the URL (scheme, host, port, path, query) ------------
  Result<Url> url_result = Url::parse(url_.data(), url_.size() - 1);
  if (url_result.is_err()) return Result<Response>::err(url_result.error());

  const Url& target = url_result.value();

  // ---- Step 3: Resolve host and path from parsed URL ----------------------
  if (target.host.empty())
    return Result<Response>::err(
        Error(Status::INVALID_ARGUMENT, "URL has no host"));

  int port = target.port;
  if (port == 0) port = (target.scheme == StringView("https")) ? 443 : 80;

  host_.clear();
  host_.append(target.host.data(), target.host.size());
  host_.append('\0');

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
  path_.append('\0');

  // ---- Step 4: Create a TCP socket ----------------------------------------
  Result<Socket*> socket_result = SocketFactory::create(host_.data(), port);
  if (socket_result.is_err())
    return Result<Response>::err(socket_result.error());
  Socket* sock = socket_result.value();

  // ---- Step 5: Connect to remote host -------------------------------------
  Status connect_status = sock->connect(host_.data(), port, timeout_ms_);
  if (connect_status != Status::OK) {
    SocketFactory::destroy(sock);
    return Result<Response>::err(Error(connect_status, "failed to connect"));
  }

  // ---- Step 6: Build and serialize the HTTP request -----------------------
  HttpRequest req;
  req.method = method_;
  req.url = StringView(path_.data(), path_.size() - 1);
  req.version = Version::HTTP_1_1;
  req.num_headers = 0;
  req.body = nullptr;

  for (size_t i = 0; i < num_headers_; ++i)
    req.headers[req.num_headers++] = headers_[i];

  if (!HasHeader("Host")) {
    req.headers[req.num_headers].name = StringView("Host", 4);
    req.headers[req.num_headers].value =
        StringView(host_.data(), host_.size() - 1);
    ++req.num_headers;
  }

  if (body_.size() > 0 && !HasHeader("Content-Length")) {
    char cl_buf[32];
    size_t cl_len = FormatDecimal(cl_buf, sizeof(cl_buf),
                                  static_cast<uint32_t>(body_.size()));
    size_t cl_off = header_storage_.size();
    header_storage_.append(cl_buf, cl_len);
    header_storage_.append('\0');
    req.headers[req.num_headers].name = StringView("Content-Length", 14);
    req.headers[req.num_headers].value =
        StringView(header_storage_.data() + cl_off, cl_len);
    ++req.num_headers;
  }

  Buffer* body_ptr = (body_.size() > 0) ? &body_ : nullptr;
  req.body = body_ptr;

  Buffer wire_buffer;
  Status ser_status = req.serialize(wire_buffer);
  if (ser_status != Status::OK) {
    SocketFactory::destroy(sock);
    return Result<Response>::err(Error(ser_status, "failed to serialize"));
  }

  // ---- Step 7: Send the serialized request over the socket -----------------
  Result<size_t> send_result =
      sock->send(wire_buffer.data(), wire_buffer.size());
  if (send_result.is_err()) {
    SocketFactory::destroy(sock);
    return Result<Response>::err(send_result.error());
  }

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

  // ---- Step 8: Receive the full response from the socket -------------------
  Buffer recv_buf;
  const size_t kRecvChunkSize = 4096;
  char chunk[kRecvChunkSize];

  for (;;) {
    Result<size_t> nread = sock->recv(chunk, kRecvChunkSize);
    if (nread.is_err()) {
      SocketFactory::destroy(sock);
      return Result<Response>::err(nread.error());
    }
    if (nread.value() == 0) break;
    recv_buf.append(chunk, nread.value());
  }

  sock->close();
  SocketFactory::destroy(sock);

  // ---- Step 9: Parse the HTTP response and populate Response struct --------
  Buffer header_storage;
  Result<HttpResponse> parse_result =
      HttpResponse::parse(recv_buf.data(), recv_buf.size(), header_storage);
  if (parse_result.is_err()) return Result<Response>::err(parse_result.error());

  HttpResponse& http_result = parse_result.value();
  Response resp;
  resp.status_code_ = http_result.status_code;
  resp.version_ = http_result.version;
  resp.num_headers_ = http_result.num_headers;
  resp.storage_ = static_cast<Buffer&&>(header_storage);

  for (size_t i = 0; i < http_result.num_headers; ++i)
    resp.headers_[i] = http_result.headers[i];

  resp.body_ = static_cast<Buffer&&>(http_result.body);

  return Result<Response>::ok(static_cast<Response&&>(resp));
}

/** @brief Checks whether a header with the given name has already been set
 *        on this request (case-insensitive ASCII comparison).
 * @param name  Null-terminated header name to look up.
 * @return true if a matching header exists.
 */
bool Request::HasHeader(const char* name) const {
  if (name == nullptr) return false;
  size_t name_len = std::strlen(name);
  for (size_t i = 0; i < num_headers_; ++i) {
    if (headers_[i].name.size() == name_len &&
        CaselessCompare(headers_[i].name.data(), name, name_len))
      return true;
  }
  return false;
}

}  // namespace xnet
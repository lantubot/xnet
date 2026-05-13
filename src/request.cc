// XNet Request — 编译单元：请求发送与响应解析的实现

#include "xnet/request.h"

namespace xnet {

// ============================================================================
// Request::perform — 执行 HTTP 请求的完整实现
// ============================================================================
Result<Response> Request::perform() {
  // --- 1. 解析 URL -----------------------------------------------------------
  if (url_.empty()) {
    return Result<Response>::err(
        Error(Status::INVALID_ARGUMENT, "request URL is not set"));
  }

  Result<Url> url_result = Url::parse(url_.data(), url_.size() - 1);  // -1 减去 \0
  if (url_result.is_err()) {
    return Result<Response>::err(url_result.error());
  }

  const Url& target = url_result.value();

  // --- 2. 确定主机名和端口 ---------------------------------------------------
  if (target.host.empty()) {
    return Result<Response>::err(
        Error(Status::INVALID_ARGUMENT, "URL has no host"));
  }

  // 基于 scheme 确定默认端口
  int port = target.port;
  if (port == 0) {
    if (target.scheme == StringView("https")) {
      port = 443;
    } else {
      port = 80;  // http 及其他协议的默认端口
    }
  }

  // 构建以 null 结尾的主机字符串
  host_.clear();
  host_.append(target.host.data(), target.host.size());
  host_.append('\0');

  // 构建请求路径（如果为空则默认为 "/"）
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
  // 为下面的 StringView 构造添加 null 终止符
  path_.append('\0');

  // --- 3. 创建并连接 socket --------------------------------------------------
  Result<Socket*> socket_result = SocketFactory::create(host_.data(), port);
  if (socket_result.is_err()) {
    return Result<Response>::err(socket_result.error());
  }
  Socket* sock = socket_result.value();

  // 带超时连接
  Status connect_status = sock->connect(host_.data(), port, timeout_ms_);
  if (connect_status != Status::OK) {
    SocketFactory::destroy(sock);
    return Result<Response>::err(
        Error(connect_status, "failed to connect"));
  }

  // --- 4. 构建 HTTP 请求 -----------------------------------------------------
  HttpRequest req;
  req.method = method_;
  req.url = StringView(path_.data(), path_.size() - 1);  // 排除 \0
  req.version = Version::HTTP_1_1;
  req.num_headers = 0;
  req.body = nullptr;

  // 从构建器复制头部
  for (size_t i = 0; i < num_headers_; ++i) {
    req.headers[req.num_headers++] = headers_[i];
  }

  // 如果尚未存在，添加 Host 头部
  if (!HasHeader("Host")) {
    req.headers[req.num_headers].name = StringView("Host", 4);
    req.headers[req.num_headers].value =
        StringView(host_.data(), host_.size() - 1);
    ++req.num_headers;
  }

  // 对非空 body 添加 Content-Length 头部
  if (body_.size() > 0 && !HasHeader("Content-Length")) {
    // 使用栈缓冲区，足够容纳任何 64 位整数
    char cl_buf[32];
    size_t cl_len = FormatDecimal(cl_buf, sizeof(cl_buf),
                                  static_cast<unsigned long long>(body_.size()));
    // 将格式化后的字符串存入 header_storage_ 以保证其生命周期
    size_t cl_off = header_storage_.size();
    header_storage_.append(cl_buf, cl_len);
    header_storage_.append('\0');
    req.headers[req.num_headers].name = StringView("Content-Length", 14);
    req.headers[req.num_headers].value =
        StringView(header_storage_.data() + cl_off, cl_len);
    ++req.num_headers;
  }

  // 设置 body（如果存在）
  Buffer* body_ptr = nullptr;
  if (body_.size() > 0) {
    body_ptr = &body_;
  }
  req.body = body_ptr;

  // --- 5. 序列化请求 ---------------------------------------------------------
  Buffer wire_buffer;
  Status ser_status = req.serialize(wire_buffer);
  if (ser_status != Status::OK) {
    SocketFactory::destroy(sock);
    return Result<Response>::err(
        Error(ser_status, "failed to serialize request"));
  }

  // --- 6. 发送请求 -----------------------------------------------------------
  Result<size_t> send_result = sock->send(wire_buffer.data(), wire_buffer.size());
  if (send_result.is_err()) {
    SocketFactory::destroy(sock);
    return Result<Response>::err(send_result.error());
  }

  // 持续发送直到所有数据传输完毕
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

  // --- 7. 接收响应 -----------------------------------------------------------
  Buffer recv_buf;

  // 持续接收数据，直到远端关闭连接或发生错误。
  // 生产环境中会解析 Content-Length 或 Transfer-Encoding: chunked 来确定
  // 精确的响应大小，为简单起见我们读到 EOF（常见于 HTTP/1.0 和
  // 使用了 Connection: close 的 HTTP/1.1 服务器）。
  const size_t kRecvChunkSize = 4096;
  char chunk[kRecvChunkSize];

  for (;;) {
    Result<size_t> nread = sock->recv(chunk, kRecvChunkSize);
    if (nread.is_err()) {
      SocketFactory::destroy(sock);
      return Result<Response>::err(nread.error());
    }
    if (nread.value() == 0) {
      // 来自服务器的干净 EOF — 所有数据已接收完毕
      break;
    }
    recv_buf.append(chunk, nread.value());
  }

  // 关闭连接（已收到响应）
  sock->close();
  SocketFactory::destroy(sock);

  // --- 8. 解析响应 -----------------------------------------------------------
  // HttpResponse::parse() 需要一个单独的头部存储缓冲区
  Buffer header_storage;
  Result<HttpResponse> parse_result =
      HttpResponse::parse(recv_buf.data(), recv_buf.size(), header_storage);
  if (parse_result.is_err()) {
    return Result<Response>::err(parse_result.error());
  }

  // --- 9. 构建 Response ------------------------------------------------------
  HttpResponse& http_result = parse_result.value();
  Response resp;
  resp.status_code_ = http_result.status_code;
  resp.version_ = http_result.version;
  resp.num_headers_ = http_result.num_headers;

  // 移动头部存储，使 StringViews 保持有效
  resp.storage_ = static_cast<Buffer&&>(header_storage);

  // 复制头部 StringViews — 它们现在指向 resp.storage_
  for (size_t i = 0; i < http_result.num_headers; ++i) {
    resp.headers_[i] = http_result.headers[i];
  }

  // 移动响应体
  resp.body_ = static_cast<Buffer&&>(http_result.body);

  return Result<Response>::ok(static_cast<Response&&>(resp));
}

// ============================================================================
// Request::HasHeader — 检查给定名称的头部是否已存在
// ============================================================================
bool Request::HasHeader(const char* name) const {
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

// ============================================================================
// Request::CaselessCompare — 不区分大小写的 ASCII 字符串比较
// ============================================================================
constexpr bool Request::CaselessCompare(const char* a, const char* b, size_t len) {
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

// ============================================================================
// Request::FormatDecimal — 将无符号整数格式化为十进制字符串
// ============================================================================
constexpr size_t Request::FormatDecimal(char* buf, size_t buf_size,
                              unsigned long long val) {
  if (buf == nullptr || buf_size == 0) return 0;

  // 反向写入数字
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

  // 反转写入输出缓冲区
  size_t written = idx;
  if (written > buf_size) written = buf_size;
  for (size_t i = 0; i < written; ++i) {
    buf[i] = tmp[idx - 1 - i];
  }
  return written;
}

}  // namespace xnet

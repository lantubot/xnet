// XNet HTTP — 编译单元

#include "xnet/http.h"

#include <cstring>

namespace xnet {

// ============================================================================
// HttpRequest::serialize
// ============================================================================
Status HttpRequest::serialize(Buffer& out) const {
  // --- 请求行 ---------------------------------------------------------------
  out.append(to_string(method), std::strlen(to_string(method)));
  out.append(' ');

  out.append(url.data(), url.size());
  out.append(' ');

  out.append(to_string(version), std::strlen(to_string(version)));
  out.append('\r');
  out.append('\n');

  // --- 头部 ----------------------------------------------------------------
  for (size_t i = 0; i < num_headers; ++i) {
    out.append(headers[i].name.data(), headers[i].name.size());
    out.append(':');
    out.append(' ');
    out.append(headers[i].value.data(), headers[i].value.size());
    out.append('\r');
    out.append('\n');
  }

  // --- 空行（头部结束标志）-------------------------------------------------
  out.append('\r');
  out.append('\n');

  // --- 体 -------------------------------------------------------------------
  if (body != nullptr && body->size() > 0) {
    out.append(body->data(), body->size());
  }

  return Status::OK;
}

// ============================================================================
// HttpResponse::parse
// ============================================================================
Result<HttpResponse> HttpResponse::parse(const char* data, size_t len,
                                         Buffer& header_storage) {
  // --- 初始化默认响应 -------------------------------------------------------
  HttpResponse resp;
  resp.version = Version::HTTP_1_1;
  resp.status_code = 0;
  resp.num_headers = 0;

  if (data == nullptr || len == 0) {
    return Result<HttpResponse>::err(
        Error(Status::INVALID_ARGUMENT, "empty response data"));
  }

  size_t pos = 0;

  // ==========================================================================
  // 1. 状态行："HTTP/1.1 200 OK\r\n"
  // ==========================================================================

  // 找到第一个空格——之前的部分即为协议版本。
  size_t space1 = pos;
  while (space1 < len && data[space1] != ' ') {
    ++space1;
  }
  if (space1 == len || space1 == pos) {
    return Result<HttpResponse>::err(Error(
        Status::PROTOCOL_ERROR, "cannot find HTTP version in status line"));
  }

  // 确定版本。
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

  // 找到第二个空格——状态码位于两个空格之间。
  size_t space2 = pos;
  while (space2 < len && data[space2] != ' ') {
    ++space2;
  }
  if (space2 == len || space2 == pos) {
    return Result<HttpResponse>::err(
        Error(Status::PROTOCOL_ERROR, "cannot find status code"));
  }

  // 解析十进制状态码。
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

  // 找到终止状态行的 \r\n。
  size_t crlf = pos;
  while (crlf + 1 < len && !(data[crlf] == '\r' && data[crlf + 1] == '\n')) {
    ++crlf;
  }
  if (crlf + 1 >= len) {
    return Result<HttpResponse>::err(
        Error(Status::PROTOCOL_ERROR, "unterminated status line"));
  }

  // 将原因短语存储到 header_storage 中。
  {
    size_t rlen = crlf - pos;
    size_t roff = header_storage.size();
    header_storage.append(data + pos, rlen);
    header_storage.append('\0');
    resp.reason = StringView(header_storage.data() + roff, rlen);
  }

  pos = crlf + 2;

  // ==========================================================================
  // 2. 头部字段
  // ==========================================================================
  while (pos + 1 < len) {
    // 空行表示头部结束。
    if (data[pos] == '\r' && data[pos + 1] == '\n') {
      pos += 2;
      break;
    }

    if (resp.num_headers >= kMaxHeaders) {
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "exceeded maximum number of headers"));
    }

    // 定位分隔名称和值的冒号。
    size_t colon = pos;
    while (colon < len && data[colon] != ':') {
      ++colon;
    }
    if (colon == len || colon == pos) {
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "malformed header: missing colon"));
    }

    // 头部名称（截取至冒号位置，不修剪尾部空格）。
    {
      size_t noff = header_storage.size();
      header_storage.append(data + pos, colon - pos);
      header_storage.append('\0');
      resp.headers[resp.num_headers].name =
          StringView(header_storage.data() + noff, colon - pos);
    }

    // 跳过冒号及值开头的空白字符。
    size_t val_start = colon + 1;
    while (val_start < len && data[val_start] == ' ') {
      ++val_start;
    }

    // 找到终止此头部的 \r\n。
    size_t eol = val_start;
    while (eol + 1 < len && !(data[eol] == '\r' && data[eol + 1] == '\n')) {
      ++eol;
    }
    if (eol + 1 >= len) {
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "unterminated header line"));
    }

    // 头部值。
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
  // 3. 体——头部终止符之后的所有剩余数据
  // ==========================================================================
  if (pos < len) {
    resp.body.append(data + pos, len - pos);
  }

  return Result<HttpResponse>::ok(move(resp));
}

}  // namespace xnet

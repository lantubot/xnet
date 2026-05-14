#include "xnet/http.h"

/// @file src/http.cc
/// @brief HTTP request serialization and response parsing implementation.

#include <cstring>

namespace xnet {

/// @brief Serialize this HTTP request into @p out in HTTP/1.x wire format.
/// Appends the request line (METHOD URL VERSION), headers, empty-line
/// delimiter, and optional body directly to the buffer.
/// @param out The buffer to write the serialized request into.
/// @return Status::OK on success.
Status HttpRequest::serialize(Buffer& out) const {
  out.append(to_string(method), std::strlen(to_string(method)));
  out.append(' ');
  out.append(url.data(), url.size());
  out.append(' ');
  out.append(to_string(version), std::strlen(to_string(version)));
  out.append('\r');
  out.append('\n');

  for (size_t i = 0; i < num_headers; ++i) {
    out.append(headers[i].name.data(), headers[i].name.size());
    out.append(':');
    out.append(' ');
    out.append(headers[i].value.data(), headers[i].value.size());
    out.append('\r');
    out.append('\n');
  }

  out.append('\r');
  out.append('\n');

  if (body != nullptr && body->size() > 0)
    out.append(body->data(), body->size());

  return Status::OK;
}

/// @brief Parse an HTTP response from raw bytes.
/// Walks @p data to extract the status line (version, status code, reason),
/// headers (storing StringViews into @p header_storage), and body. Returns an
/// error on malformed or truncated input.
/// @param data            Pointer to raw response bytes.
/// @param len             Length of the response data.
/// @param header_storage  Buffer that backs the parsed Header StringViews;
///                        must outlive the returned HttpResponse.
/// @return Result containing a fully populated HttpResponse on success, or
///         an Error describing the parse failure.
Result<HttpResponse> HttpResponse::parse(const char* data, size_t len,
                                         Buffer& header_storage) {
  HttpResponse resp;
  resp.version = Version::HTTP_1_1;
  resp.status_code = 0;
  resp.num_headers = 0;

  if (data == nullptr || len == 0)
    return Result<HttpResponse>::err(
        Error(Status::INVALID_ARGUMENT, "empty response data"));

  size_t pos = 0;

  size_t space1 = pos;
  while (space1 < len && data[space1] != ' ') ++space1;
  if (space1 == len || space1 == pos)
    return Result<HttpResponse>::err(
        Error(Status::PROTOCOL_ERROR, "cannot find HTTP version"));

  StringView ver(data + pos, space1 - pos);
  if (ver == StringView("HTTP/1.0"))
    resp.version = Version::HTTP_1_0;
  else if (ver == StringView("HTTP/1.1"))
    resp.version = Version::HTTP_1_1;
  else if (ver == StringView("HTTP/2.0"))
    resp.version = Version::HTTP_2_0;
  else
    return Result<HttpResponse>::err(
        Error(Status::UNSUPPORTED_PROTOCOL, "unsupported HTTP version"));

  pos = space1 + 1;

  size_t space2 = pos;
  while (space2 < len && data[space2] != ' ') ++space2;
  if (space2 == len || space2 == pos)
    return Result<HttpResponse>::err(
        Error(Status::PROTOCOL_ERROR, "cannot find status code"));

  int code = 0;
  for (size_t i = pos; i < space2; ++i) {
    if (data[i] < '0' || data[i] > '9')
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "non-digit in status code"));
    code = code * 10 + static_cast<int>(data[i] - '0');
  }
  resp.status_code = code;
  pos = space2 + 1;

  size_t crlf = pos;
  while (crlf + 1 < len && !(data[crlf] == '\r' && data[crlf + 1] == '\n'))
    ++crlf;
  if (crlf + 1 >= len)
    return Result<HttpResponse>::err(
        Error(Status::PROTOCOL_ERROR, "unterminated status line"));

  {
    size_t rlen = crlf - pos;
    size_t roff = header_storage.size();
    header_storage.append(data + pos, rlen);
    header_storage.append('\0');
    resp.reason = StringView(header_storage.data() + roff, rlen);
  }
  pos = crlf + 2;

  while (pos + 1 < len) {
    if (data[pos] == '\r' && data[pos + 1] == '\n') {
      pos += 2;
      break;
    }

    if (resp.num_headers >= kMaxHeaders)
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "exceeded max headers"));

    size_t colon = pos;
    while (colon < len && data[colon] != ':') ++colon;
    if (colon == len || colon == pos)
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "malformed header: missing colon"));

    {
      size_t noff = header_storage.size();
      header_storage.append(data + pos, colon - pos);
      header_storage.append('\0');
      resp.headers[resp.num_headers].name =
          StringView(header_storage.data() + noff, colon - pos);
    }

    size_t val_start = colon + 1;
    while (val_start < len && data[val_start] == ' ') ++val_start;

    size_t eol = val_start;
    while (eol + 1 < len && !(data[eol] == '\r' && data[eol + 1] == '\n'))
      ++eol;
    if (eol + 1 >= len)
      return Result<HttpResponse>::err(
          Error(Status::PROTOCOL_ERROR, "unterminated header line"));

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

  if (pos < len) resp.body.append(data + pos, len - pos);

  return Result<HttpResponse>::ok(move(resp));
}

}  // namespace xnet

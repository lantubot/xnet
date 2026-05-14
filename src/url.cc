#include "xnet/url.h"

/** @file url.cc
 * @brief RFC 3986 URL 解析器实现 — 百分号编码/解码、URL 组件解析
 */

namespace xnet {

/** 解析 URL 字符串。
 *
 * 按 RFC 3986 分阶段解析：
 *   1. 提取 scheme（ParseScheme）
 *   2. 处理 authority（userinfo@host:port）
 *   3. 提取 fragment（#）
 *   4. 提取 query（?）
 *   5. 提取 path
 *
 * @param str  待解析的 URL 字符串
 * @param len  字符串长度
 * @return 解析成功返回包含 Url 的 Result，格式错误返回 Error
 */
Result<Url> Url::parse(const char* str, size_t len) {
  Url url;

  if (str == nullptr || len == 0)
    return Result<Url>::err(
        Error(Status::INVALID_ARGUMENT, "URL string is null or empty"));

  size_t pos = ParseScheme(str, len, url.scheme);
  if (pos != StringView::npos && pos + 2 < len && str[pos] == ':' &&
      str[pos + 1] == '/' && str[pos + 2] == '/') {
    pos += 3;
  } else if (pos != StringView::npos && pos + 1 < len && str[pos] == ':') {
    url.path = StringView(str + pos + 1, len - pos - 1);
    return Result<Url>::ok(url);
  } else {
    url.scheme = StringView();
    pos = 0;
  }

  size_t auth_start = pos;
  size_t auth_end = pos;
  while (auth_end < len) {
    char c = str[auth_end];
    if (c == '/' || c == '?' || c == '#') break;
    ++auth_end;
  }

  if (auth_end > auth_start) {
    StringView auth(str + auth_start, auth_end - auth_start);

    size_t at_pos = auth.find('@');
    if (at_pos != StringView::npos) {
      StringView userinfo = auth.substr(0, at_pos);
      size_t ul_colon = userinfo.find(':');
      if (ul_colon != StringView::npos) {
        url.username = userinfo.substr(0, ul_colon);
        url.password = userinfo.substr(ul_colon + 1);
      } else {
        url.username = userinfo;
      }
      auth = auth.substr(at_pos + 1);
    }

    if (!auth.empty()) {
      if (auth.data()[0] == '[') {
        size_t close = FindClosingBracket(auth.data(), auth.size(), 0);
        if (close == StringView::npos)
          return Result<Url>::err(
              Error(Status::INVALID_ARGUMENT, "Unclosed IPv6 bracket in host"));
        url.host = auth.substr(1, close - 1);
        size_t after_close = close + 1;
        if (after_close < auth.size() && auth.data()[after_close] == ':')
          url.port = auth.substr(after_close + 1).to_int();
      } else {
        size_t host_colon = auth.find(':');
        if (host_colon != StringView::npos) {
          url.host = auth.substr(0, host_colon);
          url.port = auth.substr(host_colon + 1).to_int();
        } else {
          url.host = auth;
        }
      }
    }
  }

  pos = auth_end;

  size_t hash_pos = StringView::npos;
  for (size_t i = pos; i < len; ++i) {
    if (str[i] == '#') {
      hash_pos = i;
      break;
    }
  }
  if (hash_pos != StringView::npos) {
    url.fragment = StringView(str + hash_pos + 1, len - hash_pos - 1);
    len = hash_pos;
  }

  size_t query_pos = StringView::npos;
  for (size_t i = pos; i < len; ++i) {
    if (str[i] == '?') {
      query_pos = i;
      break;
    }
  }
  if (query_pos != StringView::npos) {
    url.query = StringView(str + query_pos + 1, len - query_pos - 1);
    len = query_pos;
  }

  if (pos < len) url.path = StringView(str + pos, len - pos);

  return Result<Url>::ok(url);
}

/** 查找并提取 URL scheme。
 *
 * scheme 必须以字母开头，后跟字母、数字、'+'、'-' 或 '.'，以 ':' 结尾。
 *
 * @param str     URL 字符串
 * @param len     字符串长度
 * @param scheme  输出，找到的 scheme 子串
 * @return 冒号索引，未找到时返回 npos
 */
size_t Url::ParseScheme(const char* str, size_t len, StringView& scheme) {
  if (str == nullptr || len == 0) return StringView::npos;
  char first = str[0];
  if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z')))
    return StringView::npos;

  size_t i = 1;
  while (i < len) {
    char c = str[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.') {
      ++i;
    } else if (c == ':') {
      scheme = StringView(str, i);
      return i;
    } else {
      return StringView::npos;
    }
  }
  return StringView::npos;
}

/** 查找匹配的 IPv6 右方括号。
 *
 * 从 pos 位置开始查找，str[pos] 必须为 '['。
 *
 * @param str  字符串
 * @param len  字符串长度
 * @param pos  起始位置（应指向 '['）
 * @return 匹配的 ']' 索引，未找到返回 npos
 */
size_t Url::FindClosingBracket(const char* str, size_t len, size_t pos) {
  if (pos >= len || str[pos] != '[') return StringView::npos;
  for (size_t i = pos + 1; i < len; ++i)
    if (str[i] == ']') return i;
  return StringView::npos;
}

/** 对输入字符串进行百分号编码。
 *
 * 非保留字符（A-Z、a-z、0-9、'-'、'.'、'_'、'~'）原样输出；
 * 其余字符编码为 %XX（大写十六进制）。
 *
 * @param input       待编码的输入
 * @param output      输出缓冲区指针
 * @param output_size 输出缓冲区大小
 * @param written     可选，接收实际写入字节数
 * @return 成功返回 true，缓冲区不足返回 false
 */
bool Url::encode(const StringView& input, char* output, size_t output_size,
                 size_t* written) {
  static const char kHexDigits[] = "0123456789ABCDEF";
  if (output == nullptr || output_size == 0) {
    if (written != nullptr) *written = 0;
    return false;
  }
  size_t out_pos = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(input[i]);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
        c == '~') {
      if (out_pos >= output_size) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      output[out_pos++] = static_cast<char>(c);
    } else {
      if (out_pos + 3 > output_size) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      output[out_pos++] = '%';
      output[out_pos++] = kHexDigits[c >> 4];
      output[out_pos++] = kHexDigits[c & 0x0F];
    }
  }
  if (written != nullptr) *written = out_pos;
  return true;
}

/** 对输入字符串进行百分号解码。
 *
 * %XX 序列解码为对应字节；'+' 解码为空格。
 * 无效的 % 序列导致解码失败。
 *
 * @param input       待解码的输入
 * @param output      输出缓冲区指针
 * @param output_size 输出缓冲区大小
 * @param written     可选，接收实际写入字节数
 * @return 成功返回 true，格式错误或缓冲区不足返回 false
 */
bool Url::decode(const StringView& input, char* output, size_t output_size,
                 size_t* written) {
  if (output == nullptr || output_size == 0) {
    if (written != nullptr) *written = 0;
    return false;
  }
  size_t out_pos = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (c == '%') {
      if (i + 2 >= input.size()) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      int high = HexValue(input[i + 1]);
      int low = HexValue(input[i + 2]);
      if (high < 0 || low < 0) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      if (out_pos >= output_size) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      output[out_pos++] = static_cast<char>((high << 4) | low);
      i += 2;
    } else if (c == '+') {
      if (out_pos >= output_size) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      output[out_pos++] = ' ';
    } else {
      if (out_pos >= output_size) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      output[out_pos++] = c;
    }
  }
  if (written != nullptr) *written = out_pos;
  return true;
}

/** 十六进制字符转整数值。
 *
 * @param c  十六进制字符
 * @return 0-15，无效字符返回 -1
 */
constexpr int Url::HexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}  // namespace xnet

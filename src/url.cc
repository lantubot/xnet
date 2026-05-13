// XNet URL 解析器 — 实现文件

#include "xnet/url.h"

namespace xnet {

// ============================================================================
// Url::parse — 解析 URL 字符串（RFC 3986）
// ============================================================================
Result<Url> Url::parse(const char* str, size_t len) {
  Url url;

  if (str == nullptr || len == 0) {
    return Result<Url>::err(
        Error(Status::INVALID_ARGUMENT, "URL string is null or empty"));
  }

  // --- 1. 解析 scheme -------------------------------------------------------
  size_t pos = ParseScheme(str, len, url.scheme);
  if (pos != StringView::npos && pos + 2 < len && str[pos] == ':' &&
      str[pos + 1] == '/' && str[pos + 2] == '/') {
    // 带 "://" 的层次化 URL
    pos += 3;  // 跳过 "://"
  } else if (pos != StringView::npos && pos + 1 < len && str[pos] == ':') {
    // 不透明 scheme（例如 "mailto:user@example.com"）— 无 authority。
    // 冒号后的所有内容即为 path。
    url.path = StringView(str + pos + 1, len - pos - 1);
    return Result<Url>::ok(url);
  } else {
    // 未检测到 scheme — 整个字符串为相对引用。
    url.scheme = StringView();
    pos = 0;
  }

  // --- 2. 将 authority 与 path+query+fragment 分离 ---------------------------
  // authority 一直延伸到 '/'、'?'、'#' 或字符串末尾。
  size_t auth_start = pos;
  size_t auth_end = pos;
  while (auth_end < len) {
    char c = str[auth_end];
    if (c == '/' || c == '?' || c == '#') break;
    ++auth_end;
  }

  // --- 3. 解析 authority：[userinfo@]host[:port] ----------------------------
  if (auth_end > auth_start) {
    StringView auth(str + auth_start, auth_end - auth_start);

    // 在 '@' 处分割 userinfo
    size_t at_pos = auth.find('@');
    if (at_pos != StringView::npos) {
      // 存在 userinfo
      StringView userinfo = auth.substr(0, at_pos);
      size_t ul_colon = userinfo.find(':');
      if (ul_colon != StringView::npos) {
        url.username = userinfo.substr(0, ul_colon);
        url.password = userinfo.substr(ul_colon + 1);
      } else {
        url.username = userinfo;
      }
      auth = auth.substr(at_pos + 1);  // 剩余部分为 host[:port]
    }

    // 从剩余 authority 中解析 host 和可选的 port
    if (!auth.empty()) {
      if (auth.data()[0] == '[') {
        // IPv6 字面量（加方括号）
        size_t close = FindClosingBracket(auth.data(), auth.size(), 0);
        if (close == StringView::npos) {
          return Result<Url>::err(
              Error(Status::INVALID_ARGUMENT,
                    "Unclosed IPv6 bracket in URL host"));
        }
        url.host = auth.substr(1, close - 1);  // 去掉方括号
        // 检查 ']' 后是否有端口
        size_t after_close = close + 1;
        if (after_close < auth.size() && auth.data()[after_close] == ':') {
          StringView port_str = auth.substr(after_close + 1);
          url.port = port_str.to_int();
        }
      } else {
        // 普通主机 — 第一个 ':' 分隔 host 和 port
        size_t host_colon = auth.find(':');
        if (host_colon != StringView::npos) {
          url.host = auth.substr(0, host_colon);
          StringView port_str = auth.substr(host_colon + 1);
          url.port = port_str.to_int();
        } else {
          url.host = auth;
        }
      }
    }
  }

  // --- 4. 解析 path、query、fragment -----------------------------------------
  pos = auth_end;

  // fragment：'#' 后的所有内容
  size_t hash_pos = StringView::npos;
  for (size_t i = pos; i < len; ++i) {
    if (str[i] == '#') {
      hash_pos = i;
      break;
    }
  }

  if (hash_pos != StringView::npos) {
    url.fragment = StringView(str + hash_pos + 1, len - hash_pos - 1);
    len = hash_pos;  // 为 path/query 解析截断末尾
  }

  // query：'?' 后的所有内容（在 fragment 之前）
  size_t query_pos = StringView::npos;
  for (size_t i = pos; i < len; ++i) {
    if (str[i] == '?') {
      query_pos = i;
      break;
    }
  }

  if (query_pos != StringView::npos) {
    url.query = StringView(str + query_pos + 1, len - query_pos - 1);
    len = query_pos;  // 为 path 解析截断末尾
  }

  // path：剩余所有内容
  if (pos < len) {
    url.path = StringView(str + pos, len - pos);
  }

  return Result<Url>::ok(url);
}

// ============================================================================
// Url::ParseScheme — RFC 3986 第 3.1 节
// ============================================================================
size_t Url::ParseScheme(const char* str, size_t len,
                        StringView& scheme) {
  if (str == nullptr || len == 0) return StringView::npos;

  // 首字符必须是字母。
  char first = str[0];
  if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z'))) {
    return StringView::npos;
  }

  size_t i = 1;
  while (i < len) {
    char c = str[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.') {
      ++i;
    } else if (c == ':') {
      scheme = StringView(str, i);
      return i;  // 冒号的索引
    } else {
      return StringView::npos;
    }
  }

  return StringView::npos;  // 未找到冒号
}

// ============================================================================
// Url::FindClosingBracket — 查找与 |pos| 处 '[' 匹配的 ']'
// ============================================================================
size_t Url::FindClosingBracket(const char* str, size_t len,
                               size_t pos) {
  if (pos >= len || str[pos] != '[') return StringView::npos;
  for (size_t i = pos + 1; i < len; ++i) {
    if (str[i] == ']') return i;
  }
  return StringView::npos;
}

// ============================================================================
// Url::encode — 对字符串进行百分号编码
// ============================================================================
bool Url::encode(const StringView& input, char* output,
                 size_t output_size, size_t* written) {
  static const char kHexDigits[] = "0123456789ABCDEF";

  if (output == nullptr || output_size == 0) {
    if (written != nullptr) *written = 0;
    return false;
  }

  size_t out_pos = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(input[i]);

    // 非保留字符（RFC 3986 §2.3）：ALPHA / DIGIT / '-' / '.' / '_' / '~'
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
        c == '~') {
      if (out_pos >= output_size) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      output[out_pos++] = static_cast<char>(c);
    } else {
      // 百分号编码：%HH
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

// ============================================================================
// Url::decode — 对字符串进行百分号解码
// ============================================================================
bool Url::decode(const StringView& input, char* output,
                 size_t output_size, size_t* written) {
  if (output == nullptr || output_size == 0) {
    if (written != nullptr) *written = 0;
    return false;
  }

  size_t out_pos = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];

    if (c == '%') {
      // 百分号编码序列：%HH
      if (i + 2 >= input.size()) {
        // 截断的百分号序列
        if (written != nullptr) *written = out_pos;
        return false;
      }
      int high = HexValue(input[i + 1]);
      int low  = HexValue(input[i + 2]);
      if (high < 0 || low < 0) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      if (out_pos >= output_size) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      output[out_pos++] = static_cast<char>((high << 4) | low);
      i += 2;  // 跳过两个十六进制位
    } else if (c == '+') {
      // application/x-www-form-urlencoded：'+' 解码为空格
      if (out_pos >= output_size) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      output[out_pos++] = ' ';
    } else {
      // 原文字符
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

// ============================================================================
// Url::HexValue — 将十六进制字符 [0-9a-fA-F] 转换为整数 [0..15]
// ============================================================================
constexpr int Url::HexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}  // namespace xnet

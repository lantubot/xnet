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

#ifndef XNET_URL_H_
#define XNET_URL_H_

#include "xnet/error.h"
#include "xnet/string_view.h"

#include <stddef.h>

namespace xnet {

// ============================================================================
// Url — a parsed URL (RFC 3986)
//
// All members are StringView references into the original input string.  The
// caller must ensure the input outlives this Url object.  No heap allocation
// is performed during parsing.
//
// Example:
//   Result<Url> r = Url::parse("https://user:pass@host:443/path?q=1#frag", 38);
//   if (r.is_ok()) { ... use r.value().scheme, .host, .port, etc. ... }
// ============================================================================
class Url {
 public:
  // -- URL components --------------------------------------------------------

  StringView scheme;     // e.g. "http", "https" — empty if no scheme
  StringView host;       // e.g. "example.com", "::1" (IPv6 without brackets)
  StringView path;       // e.g. "/index.html", empty if absent
  StringView query;      // e.g. "q=hello&n=1", empty if absent
  StringView fragment;   // e.g. "section-2", empty if absent
  StringView username;   // e.g. "user", empty if absent
  StringView password;   // e.g. "pass", empty if absent
  int port = 0;          // 0 means "not specified"

  // -- Parsing --------------------------------------------------------------
  //
  // Parses |str| (of length |len|) as a URL.  Returns the parsed Url on
  // success, or an Error on malformed input (e.g. empty/null string).
  //
  // Accepted forms:
  //   scheme://authority[/path][?query][#fragment]
  //   scheme:opaque-part                         (scheme with no "://")
  //   /path[?query][#fragment]                   (relative, no scheme)
  //   path[?query][#fragment]                    (relative, no leading /)
  //   ?query[#fragment]
  //   #fragment
  //
  // IPv6 host addresses MUST be bracketed: http://[::1]:8080/path
  static Result<Url> parse(const char* str, size_t len);

  // -- Percent-encoding -----------------------------------------------------
  //
  // Percent-encodes |input|, writing into |output| (|output_size| bytes).
  // Returns true on success; false if buffer is too small.
  // If non-null, |written| receives the number of bytes written (no null
  // terminator is appended).  Unreserved characters (A-Z, a-z, 0-9, - . _ ~)
  // are passed through; everything else becomes %XX (uppercase hex).
  static bool encode(const StringView& input, char* output,
                     size_t output_size, size_t* written = nullptr);

  // Percent-decodes |input|, writing into |output| (|output_size| bytes).
  // Returns true on success; false on malformed hex sequences or buffer too
  // small.  '+' is decoded as space (application/x-www-form-urlencoded
  // convention).  If non-null, |written| receives the number of bytes written.
  static bool decode(const StringView& input, char* output,
                     size_t output_size, size_t* written = nullptr);

 private:
  // Locate the scheme (if any) and return the index of the colon after it.
  // Sets |scheme| to the scheme substring on success.  Returns npos if no
  // scheme is found (RFC 3986 section 3.1).
  static size_t ParseScheme(const char* str, size_t len,
                            StringView& scheme);

  // Find the matching closing bracket for an IPv6 literal starting at |pos|.
  // Returns the index of ']' or npos if malformed.
  static size_t FindClosingBracket(const char* str, size_t len, size_t pos);

  // Convert a hex digit character [0-9a-fA-F] to its integer value.
  // Returns -1 on invalid input.
  static int HexValue(char c);
};

// ============================================================================
// Implementation
// ============================================================================

inline Result<Url> Url::parse(const char* str, size_t len) {
  Url url;

  if (str == nullptr || len == 0) {
    return Result<Url>::err(
        Error(Status::INVALID_ARGUMENT, "URL string is null or empty"));
  }

  // --- 1. Parse scheme -----------------------------------------------------
  size_t pos = ParseScheme(str, len, url.scheme);
  if (pos != StringView::npos && pos + 2 < len && str[pos] == ':' &&
      str[pos + 1] == '/' && str[pos + 2] == '/') {
    // Hierarchical URL with "://"
    pos += 3;  // skip "://"
  } else if (pos != StringView::npos && pos + 1 < len && str[pos] == ':') {
    // Opaque scheme (e.g. "mailto:user@example.com") — no authority.
    // Everything after the colon is the path.
    url.path = StringView(str + pos + 1, len - pos - 1);
    return Result<Url>::ok(url);
  } else {
    // No scheme detected — entire string is a relative reference.
    url.scheme = StringView();
    pos = 0;
  }

  // --- 2. Split authority from path+query+fragment -------------------------
  // Authority extends until '/', '?', '#', or end of string.
  size_t auth_start = pos;
  size_t auth_end = pos;
  while (auth_end < len) {
    char c = str[auth_end];
    if (c == '/' || c == '?' || c == '#') break;
    ++auth_end;
  }

  // --- 3. Parse authority: [userinfo@]host[:port] --------------------------
  if (auth_end > auth_start) {
    StringView auth(str + auth_start, auth_end - auth_start);

    // Split at '@' for userinfo
    size_t at_pos = auth.find('@');
    if (at_pos != StringView::npos) {
      // Userinfo present
      StringView userinfo = auth.substr(0, at_pos);
      size_t ul_colon = userinfo.find(':');
      if (ul_colon != StringView::npos) {
        url.username = userinfo.substr(0, ul_colon);
        url.password = userinfo.substr(ul_colon + 1);
      } else {
        url.username = userinfo;
      }
      auth = auth.substr(at_pos + 1);  // remaining is host[:port]
    }

    // Parse host and optional port from remaining authority
    if (!auth.empty()) {
      if (auth.data()[0] == '[') {
        // IPv6 literal (bracketed)
        size_t close = FindClosingBracket(auth.data(), auth.size(), 0);
        if (close == StringView::npos) {
          return Result<Url>::err(
              Error(Status::INVALID_ARGUMENT,
                    "Unclosed IPv6 bracket in URL host"));
        }
        url.host = auth.substr(1, close - 1);  // strip the brackets
        // Check for port after ']'
        size_t after_close = close + 1;
        if (after_close < auth.size() && auth.data()[after_close] == ':') {
          StringView port_str = auth.substr(after_close + 1);
          url.port = port_str.to_int();
        }
      } else {
        // Regular host — first ':' separates host from port
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

  // --- 4. Parse path, query, fragment --------------------------------------
  pos = auth_end;

  // Fragment: everything after '#'
  size_t hash_pos = StringView::npos;
  for (size_t i = pos; i < len; ++i) {
    if (str[i] == '#') {
      hash_pos = i;
      break;
    }
  }

  if (hash_pos != StringView::npos) {
    url.fragment = StringView(str + hash_pos + 1, len - hash_pos - 1);
    len = hash_pos;  // truncate end for path/query parsing
  }

  // Query: everything after '?' (before fragment)
  size_t query_pos = StringView::npos;
  for (size_t i = pos; i < len; ++i) {
    if (str[i] == '?') {
      query_pos = i;
      break;
    }
  }

  if (query_pos != StringView::npos) {
    url.query = StringView(str + query_pos + 1, len - query_pos - 1);
    len = query_pos;  // truncate end for path parsing
  }

  // Path: everything remaining
  if (pos < len) {
    url.path = StringView(str + pos, len - pos);
  }

  return Result<Url>::ok(url);
}

// ---------------------------------------------------------------------------
// Url::ParseScheme — RFC 3986 section 3.1
// ---------------------------------------------------------------------------
inline size_t Url::ParseScheme(const char* str, size_t len,
                               StringView& scheme) {
  if (str == nullptr || len == 0) return StringView::npos;

  // First character must be a letter.
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
      return i;  // index of the colon
    } else {
      return StringView::npos;
    }
  }

  return StringView::npos;  // no colon found
}

// ---------------------------------------------------------------------------
// Url::FindClosingBracket — locate ']' matching the opening '[' at |pos|
// ---------------------------------------------------------------------------
inline size_t Url::FindClosingBracket(const char* str, size_t len,
                                      size_t pos) {
  if (pos >= len || str[pos] != '[') return StringView::npos;
  for (size_t i = pos + 1; i < len; ++i) {
    if (str[i] == ']') return i;
  }
  return StringView::npos;
}

// ---------------------------------------------------------------------------
// Url::encode — percent-encode a string
// ---------------------------------------------------------------------------
inline bool Url::encode(const StringView& input, char* output,
                        size_t output_size, size_t* written) {
  static const char kHexDigits[] = "0123456789ABCDEF";

  if (output == nullptr || output_size == 0) {
    if (written != nullptr) *written = 0;
    return false;
  }

  size_t out_pos = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(input[i]);

    // Unreserved characters (RFC 3986 §2.3): ALPHA / DIGIT / '-' / '.' / '_' / '~'
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
        c == '~') {
      if (out_pos >= output_size) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      output[out_pos++] = static_cast<char>(c);
    } else {
      // Percent-encode: %HH
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

// ---------------------------------------------------------------------------
// Url::decode — percent-decode a string
// ---------------------------------------------------------------------------
inline bool Url::decode(const StringView& input, char* output,
                        size_t output_size, size_t* written) {
  if (output == nullptr || output_size == 0) {
    if (written != nullptr) *written = 0;
    return false;
  }

  size_t out_pos = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];

    if (c == '%') {
      // Percent-encoded sequence: %HH
      if (i + 2 >= input.size()) {
        // Truncated percent sequence
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
      i += 2;  // skip the two hex digits
    } else if (c == '+') {
      // application/x-www-form-urlencoded: '+' decodes to space
      if (out_pos >= output_size) {
        if (written != nullptr) *written = out_pos;
        return false;
      }
      output[out_pos++] = ' ';
    } else {
      // Literal character
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

// ---------------------------------------------------------------------------
// Url::HexValue — convert hex character [0-9a-fA-F] to integer [0..15]
// ---------------------------------------------------------------------------
inline int Url::HexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}  // namespace xnet

#endif  // XNET_URL_H_

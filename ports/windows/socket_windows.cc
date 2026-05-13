// Copyright (c) 2026 XNet Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "socket_windows.h"

#include <cstring>

namespace xnet {

static LONG g_wsa_started = 0;

static Status EnsureWsaStarted() {
  if (g_wsa_started) return Status::OK;
  WSADATA wsa_data;
  int rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (rc != 0) return Status::IO_ERROR;
  InterlockedExchange(&g_wsa_started, 1);
  return Status::OK;
}

WinTcpSocket::WinTcpSocket() : fd_(INVALID_SOCKET) {}
WinTcpSocket::~WinTcpSocket() { XNET_UNUSED(close()); }

Status WinTcpSocket::wsa_error_to_status(int wsa_err) {
  switch (wsa_err) {
    case WSAETIMEDOUT:
      return Status::TIMEOUT;
    case WSAECONNREFUSED:
      return Status::CONNECTION_REFUSED;
    case WSAECONNRESET:
    case WSAECONNABORTED:
      return Status::CONNECTION_RESET;
    case WSAHOST_NOT_FOUND:
    case WSANO_DATA:
    case WSANO_RECOVERY:
      return Status::DNS_FAILURE;
    case WSAEADDRNOTAVAIL:
    case WSAENETUNREACH:
    case WSAEHOSTUNREACH:
      return Status::UNKNOWN;
    case WSAEINVAL:
    case WSAEAFNOSUPPORT:
      return Status::UNSUPPORTED_PROTOCOL;
    case WSAENOBUFS:
    case WSA_NOT_ENOUGH_MEMORY:
      return Status::OUT_OF_MEMORY;
    default:
      return Status::IO_ERROR;
  }
}

static const char* PortToString(int port, char* buf, size_t buf_size) {
  XNET_UNUSED(buf_size);
  char tmp[6];
  size_t idx = 0;
  if (port == 0) {
    tmp[idx++] = '0';
  } else {
    int p = port;
    while (p > 0 && idx < sizeof(tmp) - 1) {
      tmp[idx++] = static_cast<char>('0' + (p % 10));
      p /= 10;
    }
  }
  size_t out = 0;
  while (idx > 0) {
    --idx;
    buf[out++] = tmp[idx];
  }
  buf[out] = '\0';
  return buf;
}

Status WinTcpSocket::connect(const char* host, int port, int timeout_ms) {
  Status ws = EnsureWsaStarted();
  if (ws != Status::OK) return ws;

  if (fd_ != INVALID_SOCKET) XNET_UNUSED(close());

  char port_str[8];
  PortToString(port, port_str, sizeof(port_str));

  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo* addr_info = nullptr;
  int rc = getaddrinfo(host, port_str, &hints, &addr_info);
  if (rc != 0 || addr_info == nullptr) return Status::DNS_FAILURE;

  SOCKET s = socket(addr_info->ai_family, addr_info->ai_socktype,
                    addr_info->ai_protocol);
  if (s == INVALID_SOCKET) {
    int wsa_err = WSAGetLastError();
    freeaddrinfo(addr_info);
    return wsa_error_to_status(wsa_err);
  }

  u_long nonblock = 1;
  rc = ioctlsocket(s, FIONBIO, &nonblock);
  if (rc != 0) {
    int wsa_err = WSAGetLastError();
    closesocket(s);
    freeaddrinfo(addr_info);
    return wsa_error_to_status(wsa_err);
  }

  rc = ::connect(s, addr_info->ai_addr, (int)addr_info->ai_addrlen);
  if (rc != 0) {
    int wsa_err = WSAGetLastError();
    if (wsa_err != WSAEWOULDBLOCK && wsa_err != WSAEINPROGRESS) {
      closesocket(s);
      freeaddrinfo(addr_info);
      return wsa_error_to_status(wsa_err);
    }
  }

  fd_set write_fds;
  FD_ZERO(&write_fds);
  FD_SET(s, &write_fds);
  fd_set except_fds;
  FD_ZERO(&except_fds);
  FD_SET(s, &except_fds);

  struct timeval tv;
  struct timeval* ptv = nullptr;
  if (timeout_ms > 0) {
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ptv = &tv;
  }

  rc = select(0, nullptr, &write_fds, &except_fds, ptv);
  if (rc == 0) {
    closesocket(s);
    freeaddrinfo(addr_info);
    return Status::TIMEOUT;
  }
  if (rc == SOCKET_ERROR) {
    int wsa_err = WSAGetLastError();
    closesocket(s);
    freeaddrinfo(addr_info);
    return wsa_error_to_status(wsa_err);
  }

  if (FD_ISSET(s, &except_fds)) {
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen);
    closesocket(s);
    freeaddrinfo(addr_info);
    return wsa_error_to_status(optval ? optval : WSAECONNREFUSED);
  }

  int optval = 0;
  socklen_t optlen = sizeof(optval);
  rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen);
  if (rc != 0 || optval != 0) {
    int err = (rc != 0) ? WSAGetLastError() : optval;
    closesocket(s);
    freeaddrinfo(addr_info);
    return wsa_error_to_status(err);
  }

  u_long block = 0;
  ioctlsocket(s, FIONBIO, &block);
  fd_ = s;
  freeaddrinfo(addr_info);
  return Status::OK;
}

Result<size_t> WinTcpSocket::send(const void* data, size_t len) {
  if (fd_ == INVALID_SOCKET)
    return Result<size_t>::err(Error(Status::IO_ERROR, "socket not connected"));
  if (data == nullptr || len == 0)
    return Result<size_t>::ok(static_cast<size_t>(0));

  const char* buf = static_cast<const char*>(data);
  size_t total_sent = 0;
  while (total_sent < len) {
    int sent =
        ::send(fd_, buf + total_sent, static_cast<int>(len - total_sent), 0);
    if (sent == SOCKET_ERROR) {
      int wsa_err = WSAGetLastError();
      if (wsa_err == WSAEWOULDBLOCK || wsa_err == WSAEINTR) continue;
      if (total_sent > 0) return Result<size_t>::ok(total_sent);
      return Result<size_t>::err(
          Error(wsa_error_to_status(wsa_err), "send failed"));
    }
    total_sent += static_cast<size_t>(sent);
  }
  return Result<size_t>::ok(total_sent);
}

Result<size_t> WinTcpSocket::recv(void* buf, size_t max_len) {
  if (fd_ == INVALID_SOCKET)
    return Result<size_t>::err(Error(Status::IO_ERROR, "socket not connected"));
  if (buf == nullptr || max_len == 0)
    return Result<size_t>::err(
        Error(Status::INVALID_ARGUMENT, "invalid recv arguments"));

  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fd_, &read_fds);

  const int kRecvTimeoutMs = 30000;
  struct timeval tv;
  tv.tv_sec = kRecvTimeoutMs / 1000;
  tv.tv_usec = (kRecvTimeoutMs % 1000) * 1000;

  int rc = select(0, &read_fds, nullptr, nullptr, &tv);
  if (rc == 0)
    return Result<size_t>::err(SocketError::timeout("recv timed out"));
  if (rc == SOCKET_ERROR)
    return Result<size_t>::err(
        Error(wsa_error_to_status(WSAGetLastError()), "select failed in recv"));

  int nread =
      ::recv(fd_, static_cast<char*>(buf), static_cast<int>(max_len), 0);
  if (nread == SOCKET_ERROR) {
    int wsa_err = WSAGetLastError();
    if (wsa_err == WSAEWOULDBLOCK || wsa_err == WSAEINTR)
      return Result<size_t>::ok(static_cast<size_t>(0));
    return Result<size_t>::err(
        Error(wsa_error_to_status(wsa_err), "recv failed"));
  }
  return Result<size_t>::ok(static_cast<size_t>(nread));
}

Status WinTcpSocket::close() {
  if (fd_ == INVALID_SOCKET) return Status::OK;
  ::shutdown(fd_, SD_BOTH);
  if (closesocket(fd_) == SOCKET_ERROR) {
    int wsa_err = WSAGetLastError();
    fd_ = INVALID_SOCKET;
    return wsa_error_to_status(wsa_err);
  }
  fd_ = INVALID_SOCKET;
  return Status::OK;
}

Result<Socket*> SocketFactory::create(const char* host, int port) {
  XNET_UNUSED(host);
  XNET_UNUSED(port);

  Status ws = EnsureWsaStarted();
  if (ws != Status::OK)
    return Result<Socket*>::err(Error(ws, "WSAStartup failed"));

  WinTcpSocket* sock = new WinTcpSocket();
  if (sock == nullptr)
    return Result<Socket*>::err(
        Error(Status::OUT_OF_MEMORY, "failed to allocate WinTcpSocket"));

  return Result<Socket*>::ok(static_cast<Socket*>(sock));
}

}  // namespace xnet

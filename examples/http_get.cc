/** @file http_get.cc
 * @brief HTTP GET/POST 请求使用示例
 */

#include <cstdio>

#include "xnet/xnet.h"

/** @brief 打印 HTTP 响应结果
 * @param result 请求结果（包含 Response 或错误信息）
 * @param label  请求标识标签
 */
static void print_response(const xnet::Result<xnet::Response>& result,
                           const char* label) {
  printf("=== %s ===\n", label);
  if (!result.is_ok()) {
    printf("ERROR: %s\n", result.error().what());
    return;
  }

  const xnet::Response& resp = result.value();
  printf("Status: %d\n", resp.status_code());
  printf("Body (%zu bytes):\n%.*s\n", resp.body_size(),
         static_cast<int>(resp.body_size()), resp.body() ? resp.body() : "");
  printf("\n");
}

int main() {
  /** @brief 简单 GET 请求示例 */
  xnet::Result<xnet::Response> result = xnet::get("http://httpbin.org/get");
  print_response(result, "Simple GET");

  /** @brief 配置 POST 请求示例（自定义 URL、方法、头部、请求体、超时） */
  xnet::Request req;
  const char* json = R"({"hello": "xnet"})";

  xnet::Result<xnet::Response> post_result =
      req.url("http://httpbin.org/post")
          .method(xnet::Method::POST)
          .header("Content-Type", "application/json")
          .body(json, 16)
          .timeout(10000)
          .perform();

  printf("=== Configured POST ===\n");
  if (post_result.is_ok()) {
    const xnet::Response& resp = post_result.value();
    printf("Status: %d\n", resp.status_code());
    printf("Body: %s\n", resp.body() ? resp.body() : "");
  } else {
    printf("ERROR: %s\n", post_result.error().what());
  }

  return 0;
}

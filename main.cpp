#ifdef _WIN32
#pragma execution_character_set("utf-8")
#endif

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "json.h"

using namespace std;
using namespace json;

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool cond, const string& what) {
  ++g_checks;
  if (!cond) {
    ++g_failures;
    cout << "  FAIL: " << what << '\n';
  }
}

// 遍历 test/ 目录：pass* 必须解析成功，fail*（非 _EXCLUDE）必须解析失败。
void runFileSuite() {
  namespace fs = std::filesystem;
  cout << "[suite] test/ cases\n";
  for (auto& x : fs::directory_iterator("test")) {
    if (x.path().extension() != ".json") continue;
    const string name = x.path().filename().string();
    const bool is_pass = name.rfind("pass", 0) == 0;
    const bool is_excl = name.find("_EXCLUDE") != string::npos;
    if (is_excl) continue;

    Reader p;
    Value v;
    const bool ok = p.parseFile(x.path().string(), v);
    if (is_pass)
      check(ok, "pass case should parse: " + name);
    else
      check(!ok, "fail case should be rejected: " + name);
  }
}

// 解析 / 序列化行为单元测试。
void runUnitTests() {
  cout << "[unit] parser & serializer\n";
  Reader r;
  Value v;

  // 基本类型
  check(r.parse(R"(null)", v) && v.isNull(), "null");
  r = Reader();
  check(r.parse(R"(true)", v) && v.asBool(), "true");
  r = Reader();
  check(r.parse(R"(-42)", v) && v.asInt() == -42, "integer");
  r = Reader();
  check(r.parse(R"(1e2)", v) && v.asDouble() == 100.0, "exponent");
  r = Reader();
  check(r.parse(R"("hi")", v) && v.asString() == "hi", "string");
  r = Reader();
  check(r.parse(R"([1,2,3])", v) && v.isArray() && v.size() == 3, "array");
  r = Reader();
  check(r.parse(R"({"a":1})", v) && v.isObject() && v.contains("a"), "object");
  r = Reader();

  // 转义
  check(r.parse(R"("a\n\t\"b\"")", v) && v.asString() == "a\n\t\"b\"",
        "escapes");
  r = Reader();

  // 代理对
  check(r.parse(R"("\uD83D\uDE00")", v) && v.asString() == "\xF0\x9F\x98\x80",
        "surrogate pair -> U+1F600");
  r = Reader();
  check(!r.parse(R"("\uDE00")", v), "lone low surrogate rejected");
  r = Reader();
  check(!r.parse(R"("\uD800")", v), "truncated high surrogate rejected");
  r = Reader();
  check(!r.parse(R"("\uD800\u0041")", v), "high surrogate + non-low rejected");
  r = Reader();

  // 尾随内容
  check(r.parse(R"(1)", v), "number parses");
  r = Reader();
  check(!r.parse(R"(1 2)", v), "trailing content rejected");
  r = Reader();
  check(!r.parse(R"(true false)", v), "trailing token rejected");
  r = Reader();
  check(r.parse(" 1 \n\t", v), "trailing whitespace ok");
  r = Reader();

  // 数字语法
  check(!r.parse(R"(01)", v), "leading zero rejected");
  r = Reader();
  check(!r.parse(R"(-01)", v), "negative leading zero rejected");
  r = Reader();
  check(r.parse(R"(0.5)", v) && v.asDouble() == 0.5, "0.5 ok");
  r = Reader();

  // dump round-trip
  check(r.parse(R"({"name":"json","version":1,"ok":true})", v),
        "object parses");
  Reader r2;
  Value v2;
  check(r2.parse(v.dump(), v2) && v2 == v, "dump round-trip");

  // 错误信息
  Reader r3;
  check(!r3.parse(R"({)", v) && !r3.getError().empty(),
        "error message present");
}

}  // namespace

int main() {
#ifdef _WIN32
  system("chcp 65001");
#endif
  const auto start = chrono::system_clock::now();
  runFileSuite();
  runUnitTests();
  const auto end = chrono::system_clock::now();
  const auto ms =
      chrono::duration_cast<chrono::milliseconds>(end - start).count();

  cout << '\n'
       << (g_checks - g_failures) << '/' << g_checks << " checks passed";
  if (g_failures) cout << ", " << g_failures << " FAILED";
  cout << " (" << ms << " ms)\n";
  return g_failures == 0 ? 0 : 1;
}
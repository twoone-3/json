// MSVC encodes narrow string literals using the system code page by default;
// this pragma makes it emit UTF-8. clang-cl does not support the pragma.
#if defined(_MSC_VER) && !defined(__clang__)
#pragma execution_character_set("utf-8")
#endif

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

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

void note(const string& what) { cout << "  ---- " << what << '\n'; }

// Parses the given text and reports whether it was accepted.
bool parses(const string& text, Value* out = nullptr) {
  Reader r;
  Value v;
  const bool ok = r.parse(text, v);
  if (out != nullptr) *out = v;
  return ok;
}

// The dump() of a value must always be a valid JSON document again.
bool dumpIsValidJson(const Value& v) {
  const string d = v.dump();
  Reader r;
  Value v2;
  if (!r.parse(d, v2)) return false;
  return v2 == v;
}

// 端到端冒烟测试：test/ 下的每个 .json 都必须能解析，且 dump() 往返后保持一致。
//
// 这里放的是"一份文档里用齐所有 JSON 构造"的整体样例（json.org 的 pass1 模式）。
// 逐个 JSON 构造的细粒度用例由 test/jsonsuite/ 下的官方 JSONTestSuite 覆盖。
void runSmokeTest() {
  namespace fs = std::filesystem;
  if (!fs::exists("test")) {
    note("test/ not found, skipping the end-to-end documents");
    return;
  }
  cout << "[suite] test/ end-to-end documents\n";
  int count = 0;
  for (auto& x : fs::directory_iterator("test")) {
    if (!x.is_regular_file() || x.path().extension() != ".json") continue;
    const string name = x.path().filename().string();
    ++count;
    Reader p;
    Value v;
    const bool ok = p.parseFile(x.path().string(), v);
    check(ok, "end-to-end document should parse: " + name);
    if (ok)
      check(dumpIsValidJson(v),
            "end-to-end document should round-trip: " + name);
  }
  note(to_string(count) + " end-to-end document(s)");
}

// 官方 JSONTestSuite (nst/JSONTestSuite) 全量用例。
//   y_* 必须解析成功，n_* 必须被拒绝，i_* 由实现自行决定。
void runJsonTestSuite() {
  namespace fs = std::filesystem;
  const fs::path dir = "test/jsonsuite";
  if (!fs::exists(dir)) {
    note("test/jsonsuite not found, skipping the JSONTestSuite");
    return;
  }
  cout << "[suite] JSONTestSuite (test/jsonsuite)\n";

  int y_ok = 0, y_bad = 0, n_ok = 0, n_bad = 0, i_parsed = 0, i_rejected = 0;
  int roundtrip_bad = 0;
  vector<string> failures;

  for (auto& x : fs::directory_iterator(dir)) {
    if (!x.is_regular_file()) continue;
    const string name = x.path().filename().string();
    if (name.size() < 2 || name[1] != '_') continue;
    const char kind = name[0];

    Reader r;
    Value v;
    const bool ok = r.parseFile(x.path().string(), v);

    if (kind == 'y') {
      if (ok) {
        ++y_ok;
        if (!dumpIsValidJson(v)) {
          ++roundtrip_bad;
          failures.push_back("dump() is not valid JSON: " + name);
        }
      } else {
        ++y_bad;
        failures.push_back("should parse but was rejected: " + name + " (" +
                           r.getError() + ")");
      }
    } else if (kind == 'n') {
      if (!ok) {
        ++n_ok;
      } else {
        ++n_bad;
        failures.push_back("should be rejected but was accepted: " + name);
      }
    } else {
      if (ok) ++i_parsed; else ++i_rejected;
    }
  }

  note("y_ (must parse)     : " + to_string(y_ok) + "/" +
       to_string(y_ok + y_bad));
  note("n_ (must be reject) : " + to_string(n_ok) + "/" +
       to_string(n_ok + n_bad));
  note("i_ (impl. defined)  : " + to_string(i_parsed) + " parsed, " +
       to_string(i_rejected) + " rejected");
  note("dump() round trip   : " + to_string(roundtrip_bad) + " failure(s)");

  check(y_bad == 0, "JSONTestSuite: all y_ cases must parse");
  check(n_bad == 0, "JSONTestSuite: all n_ cases must be rejected");
  check(roundtrip_bad == 0, "JSONTestSuite: dump() must produce valid JSON");
  for (const auto& f : failures) check(false, "JSONTestSuite: " + f);
}

// 官方 JSONTestSuite 之外的行为测试。
//
// 划分原则：凡是 JSONTestSuite（test/jsonsuite/，95 个 y_ + 188 个 n_ 用例）
// 已经覆盖的断言都不在这里重复——JSON 语法本身的正误、各种转义与数字写法、
// 基本类型、dump() 往返等，一律交给官方套件。这里只保留它表达不了的部分：
//   * 本库的扩展：注释、嵌套深度上限、emit_utf8 输出
//   * API 层面的行为：类型判断、数值转换、getError()、重复键
//   * 曾经报告过的缺陷：缓冲区溢出、越界读、UTF-8 静默改写、整数精度丢失等
void runBehaviorTests() {
  cout << "[unit] behaviour & regression\n";

  // 1) Writer: 数字缓冲区溢出 -> 曾经输出非法 JSON
  {
    const vector<double> numbers = {
        1.7976931348623157e+308,  // DBL_MAX
        2.2250738585072014e-308,  // DBL_MIN
        -1.2312312312312312e+29, // 需要 23 个字符
        1.2345678901234567e+30,
        -0.30000000000000004,
    };
    for (double d : numbers) {
      Value n(d);
      check(dumpIsValidJson(n),
            "double dump() must stay valid JSON: " + n.dump());
      Writer w;
      w.writeValue(n);
      check(parses(w.getOutput()),
            "compact write of " + n.dump() + " must be valid JSON");
    }
  }

  // 2) Writer: NaN / Infinity 没有 JSON 表示
  {
    Value nan(std::numeric_limits<double>::quiet_NaN());
    Value inf(std::numeric_limits<double>::infinity());
    Value ninf(-std::numeric_limits<double>::infinity());
    check(nan.dump() == "null", "NaN is written as null");
    check(inf.dump() == "null", "Infinity is written as null");
    check(ninf.dump() == "null", "-Infinity is written as null");
    check(parses(nan.dump()) && parses(inf.dump()), "null is valid JSON");
  }

  // 3) 注释扩展: 注释后不能吞掉第一个字符
  {
    Reader c;
    c.allowComments();
    Value cv;
    check(c.parse("/*c*/1", cv) && cv.isNumber() && cv.asInt() == 1,
          "/*c*/1 parses to 1");
    c = Reader();
    c.allowComments();
    check(c.parse("/*c*/[1,2]", cv) && cv.isArray() && cv.size() == 2,
          "/*c*/[1,2] parses to an array");
    c = Reader();
    c.allowComments();
    check(c.parse("/*c*/{\"a\":1}", cv) && cv.isObject() && cv.contains("a"),
          "/*c*/{\"a\":1} parses to an object");
    c = Reader();
    c.allowComments();
    check(c.parse("//c\n[1,2]", cv) && cv.isArray() && cv.size() == 2,
          "//c\\n[1,2] parses to an array");
    c = Reader();
    c.allowComments();
    check(c.parse("[1] /* trailing */", cv) && cv.isArray(),
          "trailing comment is accepted");
  }

  // 4) 数字范围: RFC 8259 允许实现限制数字的范围，超出 double 表示范围的
  //    一律拒绝（见 README 的"已知限制"）。数字文法本身由官方套件的
  //    n_number_* / y_number_* 用例覆盖，这里不再重复。
  {
    check(!parses("1e400"), "out of range number rejected: 1e400");
    check(!parses("123.456e-789"),
          "underflowing number rejected: 123.456e-789");
  }

  // 5) 嵌套深度限制: 不能因为太深而崩溃
  {
    const int depths[] = {100, 500, 512};
    for (int d : depths) {
      const string deep(static_cast<size_t>(d), '[');
      check(parses(deep + string(static_cast<size_t>(d), ']')),
            "nesting depth " + to_string(d) + " accepted");
    }
    const string too_deep(100000, '[');
    check(!parses(too_deep), "excessive nesting is rejected, not a crash");

    Reader d;
    d.maxDepth(3);
    Value dv;
    check(d.parse("[[[1]]]", dv), "depth 3 accepted with maxDepth(3)");
    d = Reader();
    d.maxDepth(3);
    check(!d.parse("[[[[1]]]]", dv), "depth 4 rejected with maxDepth(3)");
  }

  // 6) 越界读: 这些输入曾经会读过缓冲区末尾。
  //    官方套件里的"不完整"输入后面总有分隔符（例如 "[nul]"），走不到
  //    缓冲区末尾这条路径；下面几个才会。
  {
    const vector<string> truncated = {
        "\"\\u12",    // \u 转义在缓冲区末尾被截断
        "\"\\uD800",  // 高代理对在缓冲区末尾被截断
    };
    for (const auto& text : truncated)
      check(!parses(text), "truncated input rejected: " + text);

    // 注释是本库的扩展，官方套件表达不了：注释没有闭合时必须安全地拒绝。
    Reader c;
    c.allowComments();
    Value cv;
    check(!c.parse("/*abc", cv), "unterminated block comment rejected");
    c = Reader();
    c.allowComments();
    check(!c.parse("//abc", cv), "unterminated line comment rejected");
  }

  // 7) Writer 的 UTF-8 解码: 不能把非法字节悄悄改写成合法字符
  {
    // 0x28 不是 continuation byte。0xC3 是一个不完整的前缀，按 Unicode 的
    // "maximal subpart" 规则替换成一个 U+FFFD，后面的 '(' 正常输出。
    // 旧实现会把它悄悄改成 U+00E8（和合法的 "A\xC3\xA8B" 输出完全一样）。
    Value bad(std::string("A\xC3\x28""B"));
    check(bad.dump() == "\"A\\ufffd(B\"",
          "invalid UTF-8 becomes U+FFFD, got: " + bad.dump());
    // 合法 UTF-8 必须原样保留
    Value good(std::string("A\xC3\xA8""B"));
    check(good.dump() == "\"A\\u00e8B\"",
          "valid UTF-8 is preserved, got: " + good.dump());
    // 超过 U+10FFFF 的码点: 0xF7 不是合法的首字节，四个字节各自替换一次
    Value too_big(std::string("\xF7\xBF\xBF\xBF"));
    check(too_big.dump() == "\"\\ufffd\\ufffd\\ufffd\\ufffd\"",
          "code point above U+10FFFF becomes U+FFFD, got: " + too_big.dump());
  }

  // 8) 解析器必须拒绝非法 UTF-8 (RFC 8259 要求文本使用 UTF-8)。
  //    官方套件只含"字节不完整"的例子（E5、FF、B9 等），下面四种
  //    编码层面的错误它一个都没有。
  {
    const vector<string> invalid = {
        "\"a\xC3\x28\"",  // 不完整的前缀后面跟了个 ASCII
        "\"\xC0\x80\"",    // 过长编码 (overlong)
        "\"\xED\xA0\x80\"",  // UTF-8 编码的代理对
        "\"\xF7\xBF\xBF\xBF\"",  // 超过 U+10FFFF 的码点
    };
    for (const auto& text : invalid)
      check(!parses(text), "invalid UTF-8 rejected");
  }

  // 9) emit_utf8 模式也不能输出非法 UTF-8
  {
    Value v(std::string("\xC3\x28"));
    const string d = v.dump(true);
    check(d == "\"\xEF\xBF\xBD(\"",
          "emit_utf8 substitutes U+FFFD, got: " + d);
  }

  // 10) 整数必须精确保留
  {
    Reader r;
    Value v;
    check(r.parse("12345678901234567890", v), "big integer parses");
    check(v.dump() == "12345678901234567890",
          "big integer survives the round trip, got: " + v.dump());
    r = Reader();
    check(r.parse("9007199254740993", v), "2^53+1 parses");
    check(v.dump() == "9007199254740993",
          "2^53+1 survives the round trip, got: " + v.dump());
    r = Reader();
    check(r.parse("-9223372036854775808", v), "INT64_MIN parses");
    check(v.dump() == "-9223372036854775808", "INT64_MIN is preserved");
    r = Reader();
    check(r.parse("18446744073709551615", v), "UINT64_MAX parses");
    check(v.dump() == "18446744073709551615", "UINT64_MAX is preserved");
    r = Reader();
    check(r.parse("-0", v) && v.asDouble() == 0.0, "-0 parses");
    check(v.dump() == "-0", "the sign of -0 is preserved, got: " + v.dump());
  }

  // 11) getError() 在没有错误时不能崩溃, 也不能返回上一次的错误
  {
    Reader r;
    Value v;
    check(!r.parse("{", v), "parse of '{' fails as expected");
    check(!r.getError().empty(), "error message is present");
    r = Reader();
    check(r.parse("1", v), "parse succeeds");
    check(r.getError().empty(), "error message is cleared after a success");
    r = Reader();
    check(r.parse("1", v), "parse succeeds");
    check(r.getError().empty(), "getError() before any error is safe");
  }

  // 12) 数值转换不能触发未定义行为
  {
    Reader r;
    Value v;
    r.parse("1e300", v);
    check(v.asInt() == std::numeric_limits<int>::max(),
          "asInt() clamps instead of overflowing");
    r = Reader();
    r.parse("-1e300", v);
    check(v.asInt() == std::numeric_limits<int>::min(),
          "asInt() clamps negative overflow");
    r = Reader();
    r.parse("-1", v);
    check(v.asUInt() == 0, "asUInt() clamps negative values");
  }

  // 13) 重复键: 最后一个生效
  //    （官方套件的 y_object_duplicated_key.json 只要求"能解析"，
  //      没有规定重复键的取值。）
  {
    Reader r;
    Value v;
    check(r.parse("{\"a\":1,\"a\":2}", v) && v["a"].asInt() == 2,
          "duplicate key: the last one wins");
  }

  // 14) 值类型判断
  {
    Value v(1);
    check(v.isNumber() && !v.isNull(), "Value(1) is a number");
    check(v.asDouble() == 1.0, "Value(1) converts to 1.0");
    check(v.asInt() == 1, "Value(1) converts to int 1");
    Value arr;
    arr.append(1);
    check(arr.isArray() && arr.size() == 1, "append(1) works");
    check(arr[0].asInt() == 1, "the appended value is readable");
  }

  // 15) 代理对: 官方套件只覆盖了"高代理对后面跟着 \uXXXX 的各种坏形态"，
  //     孤立的低代理对、以及高代理对后面跟的不是 \u 转义，都还没有对应用例。
  {
    check(!parses("\"\\uDE00\""), "lone low surrogate rejected");
    check(!parses("\"\\uD800\\u0041\""),
          "high surrogate followed by a non-low-surrogate rejected");
    check(!parses("\"\\uD800\\q\""),
          "high surrogate not followed by a \\u escape rejected");
  }
}

}  // namespace

int main() {
#ifdef _WIN32
  system("chcp 65001 > nul");
#endif
  const auto start = chrono::system_clock::now();
  runSmokeTest();
  runJsonTestSuite();
  runBehaviorTests();
  const auto end = chrono::system_clock::now();
  const auto ms =
      chrono::duration_cast<chrono::milliseconds>(end - start).count();

  cout << '\n'
       << (g_checks - g_failures) << '/' << g_checks << " checks passed";
  if (g_failures) cout << ", " << g_failures << " FAILED";
  cout << " (" << ms << " ms)\n";
  return g_failures == 0 ? 0 : 1;
}

/*
Author: twoone3
Last change: 2026.10.21
Github: https://github.com/twoone-3/json
*/
#include "json.h"

#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

// clang-format off
#define JSON_SKIP_WHITE_SPACE if(!skipWhiteSpace())return false
#define JSON_CHECK_OUT_OF_RANGE if (cur_ >= end_)return error("unexpected ending character")
#define JSON_ASSERT(expr,msg) if (!(expr))Fatal(msg)
// clang-format on

namespace json {

static void Fatal(const char* msg) {
  perror(msg);
  exit(-1);
}

// Reads a file in binary mode. The content may contain NUL bytes, so the
// length is taken from the file size instead of from strlen().
static bool ReadFile(std::string_view path, std::string& content) {
  const std::string filename(path);
  FILE* pfile = fopen(filename.c_str(), "rb");
  if (pfile == nullptr) return false;
  if (fseek(pfile, 0, SEEK_END) != 0) {
    fclose(pfile);
    return false;
  }
  const long length = ftell(pfile);
  if (length < 0) {
    fclose(pfile);
    return false;
  }
  rewind(pfile);
  content.resize(static_cast<size_t>(length));
  const size_t read = fread(&content[0], 1, content.size(), pfile);
  content.resize(read);
  fclose(pfile);
  return true;
}

namespace {

// Increments a nesting counter for the lifetime of the object.
struct DepthGuard {
  unsigned& depth;
  explicit DepthGuard(unsigned& d) : depth(d) { ++depth; }
  ~DepthGuard() { --depth; }
};

constexpr unsigned kReplacementCharacter = 0xFFFD;
constexpr unsigned kDefaultMaxDepth = 512;

}  // namespace

// ---------------------------------------------------------------- UTF-8 ----

// Decodes one UTF-8 code point starting at *cur.
//
// Returns false when the bytes at *cur are not a well formed UTF-8 sequence
// (bad continuation byte, overlong encoding, encoded surrogate, code point
// above U+10FFFF, truncated sequence). *cur is only advanced on success.
static bool DecodeUTF8(const char*& cur, const char* end, unsigned& codepoint) {
  if (cur >= end) return false;
  const unsigned char b0 = static_cast<unsigned char>(*cur);

  // 0xxxxxxx
  if (b0 < 0x80) {
    codepoint = b0;
    cur += 1;
    return true;
  }

  // 110xxxxx 10xxxxxx
  if (b0 >= 0xC2 && b0 <= 0xDF) {
    if (end - cur < 2) return false;
    const unsigned char b1 = static_cast<unsigned char>(cur[1]);
    if (b1 < 0x80 || b1 > 0xBF) return false;
    codepoint = ((b0 & 0x1Fu) << 6) | (b1 & 0x3Fu);
    cur += 2;
    return true;
  }

  // 1110xxxx 10xxxxxx 10xxxxxx
  if (b0 >= 0xE0 && b0 <= 0xEF) {
    if (end - cur < 3) return false;
    const unsigned char b1 = static_cast<unsigned char>(cur[1]);
    const unsigned char b2 = static_cast<unsigned char>(cur[2]);
    if (b1 < 0x80 || b1 > 0xBF) return false;
    if (b2 < 0x80 || b2 > 0xBF) return false;
    // overlong encoding
    if (b0 == 0xE0 && b1 < 0xA0) return false;
    // UTF-16 surrogate halves are not valid code points
    if (b0 == 0xED && b1 > 0x9F) return false;
    codepoint = ((b0 & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu);
    cur += 3;
    return true;
  }

  // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  if (b0 >= 0xF0 && b0 <= 0xF4) {
    if (end - cur < 4) return false;
    const unsigned char b1 = static_cast<unsigned char>(cur[1]);
    const unsigned char b2 = static_cast<unsigned char>(cur[2]);
    const unsigned char b3 = static_cast<unsigned char>(cur[3]);
    if (b1 < 0x80 || b1 > 0xBF) return false;
    if (b2 < 0x80 || b2 > 0xBF) return false;
    if (b3 < 0x80 || b3 > 0xBF) return false;
    // overlong encoding
    if (b0 == 0xF0 && b1 < 0x90) return false;
    // above U+10FFFF
    if (b0 == 0xF4 && b1 > 0x8F) return false;
    codepoint = ((b0 & 0x07u) << 18) | ((b1 & 0x3Fu) << 12) |
                ((b2 & 0x3Fu) << 6) | (b3 & 0x3Fu);
    cur += 4;
    return true;
  }

  // 0x80 - 0xC1 (continuation byte or overlong) and 0xF5 - 0xFF
  return false;
}

static void CodePointToUTF8(std::string& s, unsigned u) {
  if (u <= 0x7F) {
    s += static_cast<char>(u & 0xFF);
  } else if (u <= 0x7FF) {
    s += static_cast<char>(0xC0 | (0xFF & (u >> 6)));
    s += static_cast<char>(0x80 | (0x3F & u));
  } else if (u <= 0xFFFF) {
    s += static_cast<char>(0xE0 | (0xFF & (u >> 12)));
    s += static_cast<char>(0x80 | (0x3F & (u >> 6)));
    s += static_cast<char>(0x80 | (0x3F & u));
  } else {
    s += static_cast<char>(0xF0 | (0xFF & (u >> 18)));
    s += static_cast<char>(0x80 | (0x3F & (u >> 12)));
    s += static_cast<char>(0x80 | (0x3F & (u >> 6)));
    s += static_cast<char>(0x80 | (0x3F & u));
  }
}

// ---------------------------------------------------------------- Number ---

Number::Number() : kind_(Kind::kDouble), d_(0.0), i_(0), u_(0) {}

Number::Number(double value) : kind_(Kind::kDouble), d_(value), i_(0), u_(0) {}

Number::Number(int64_t value) : kind_(Kind::kInt), d_(0.0), i_(value), u_(0) {}

Number::Number(uint64_t value)
    : kind_(Kind::kUInt), d_(0.0), i_(0), u_(value) {}

Number::Kind Number::kind() const { return kind_; }

bool Number::isInteger() const { return kind_ != Kind::kDouble; }

double Number::asDouble() const {
  switch (kind_) {
    case Kind::kDouble:
      return d_;
    case Kind::kInt:
      return static_cast<double>(i_);
    case Kind::kUInt:
      return static_cast<double>(u_);
  }
  return 0.0;
}

int Number::asInt() const {
  constexpr int64_t kMin = std::numeric_limits<int>::min();
  constexpr int64_t kMax = std::numeric_limits<int>::max();
  switch (kind_) {
    case Kind::kInt:
      if (i_ < kMin) return static_cast<int>(kMin);
      if (i_ > kMax) return static_cast<int>(kMax);
      return static_cast<int>(i_);
    case Kind::kUInt:
      if (u_ > static_cast<uint64_t>(kMax)) return static_cast<int>(kMax);
      return static_cast<int>(u_);
    case Kind::kDouble:
      break;
  }
  if (std::isnan(d_)) return 0;
  if (d_ >= static_cast<double>(kMax)) return static_cast<int>(kMax);
  if (d_ <= static_cast<double>(kMin)) return static_cast<int>(kMin);
  return static_cast<int>(d_);
}

unsigned Number::asUInt() const {
  constexpr uint64_t kMax = std::numeric_limits<unsigned>::max();
  switch (kind_) {
    case Kind::kInt:
      if (i_ < 0) return 0;
      if (static_cast<uint64_t>(i_) > kMax) return static_cast<unsigned>(kMax);
      return static_cast<unsigned>(i_);
    case Kind::kUInt:
      if (u_ > kMax) return static_cast<unsigned>(kMax);
      return static_cast<unsigned>(u_);
    case Kind::kDouble:
      break;
  }
  if (!(d_ > 0.0)) return 0;  // also rejects NaN
  if (d_ >= static_cast<double>(kMax)) return static_cast<unsigned>(kMax);
  return static_cast<unsigned>(d_);
}

int64_t Number::asInt64() const {
  switch (kind_) {
    case Kind::kInt:
      return i_;
    case Kind::kUInt:
      if (u_ > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        return std::numeric_limits<int64_t>::max();
      return static_cast<int64_t>(u_);
    case Kind::kDouble:
      break;
  }
  if (std::isnan(d_)) return 0;
  constexpr double kMin = -9223372036854775808.0;  // -2^63
  constexpr double kMax = 9223372036854775808.0;   //  2^63
  if (d_ >= kMax) return std::numeric_limits<int64_t>::max();
  if (d_ <= kMin) return std::numeric_limits<int64_t>::min();
  return static_cast<int64_t>(d_);
}

uint64_t Number::asUInt64() const {
  switch (kind_) {
    case Kind::kInt:
      return static_cast<uint64_t>(i_ < 0 ? 0 : i_);
    case Kind::kUInt:
      return u_;
    case Kind::kDouble:
      break;
  }
  if (!(d_ > 0.0)) return 0;  // also rejects NaN
  constexpr double kMax = 18446744073709551616.0;  // 2^64
  if (d_ >= kMax) return std::numeric_limits<uint64_t>::max();
  return static_cast<uint64_t>(d_);
}

std::string Number::toString() const {
  // 64 bytes is far more than the longest representation of a double
  // ("-2.2250738585072014e-308" is 24 characters).
  char buffer[64];
  switch (kind_) {
    case Kind::kInt: {
      const auto result = std::to_chars(buffer, buffer + sizeof buffer, i_);
      if (result.ec != std::errc{}) return "null";
      return std::string(buffer, result.ptr);
    }
    case Kind::kUInt: {
      const auto result = std::to_chars(buffer, buffer + sizeof buffer, u_);
      if (result.ec != std::errc{}) return "null";
      return std::string(buffer, result.ptr);
    }
    case Kind::kDouble:
      break;
  }
  // NaN and Infinity have no JSON representation.
  if (!std::isfinite(d_)) return "null";
  const auto result = std::to_chars(buffer, buffer + sizeof buffer, d_);
  if (result.ec != std::errc{}) return "null";
  return std::string(buffer, result.ptr);
}

namespace {

// Exact conversions used to compare a double against an integer.
bool DoubleToInt64(double d, int64_t& out) {
  constexpr double kMin = -9223372036854775808.0;  // -2^63
  constexpr double kMax = 9223372036854775808.0;   //  2^63
  if (!(d >= kMin && d < kMax)) return false;
  if (d != std::trunc(d)) return false;
  const int64_t value = static_cast<int64_t>(d);
  if (static_cast<double>(value) != d) return false;
  out = value;
  return true;
}

bool DoubleToUInt64(double d, uint64_t& out) {
  constexpr double kMax = 18446744073709551616.0;  // 2^64
  if (!(d >= 0.0 && d < kMax)) return false;
  if (d != std::trunc(d)) return false;
  const uint64_t value = static_cast<uint64_t>(d);
  if (static_cast<double>(value) != d) return false;
  out = value;
  return true;
}

}  // namespace

bool Number::operator==(const Number& other) const {
  if (kind_ == Kind::kDouble || other.kind_ == Kind::kDouble) {
    // Compare exactly when the double happens to be an exact integer.
    if (kind_ == Kind::kDouble && other.kind_ == Kind::kDouble)
      return d_ == other.d_;
    const Number& d = (kind_ == Kind::kDouble) ? *this : other;
    const Number& i = (kind_ == Kind::kDouble) ? other : *this;
    int64_t di = 0;
    uint64_t du = 0;
    if (i.kind_ == Kind::kInt) {
      if (DoubleToInt64(d.d_, di)) return di == i.i_;
      return d.d_ == static_cast<double>(i.i_);
    }
    if (DoubleToUInt64(d.d_, du)) return du == i.u_;
    return d.d_ == static_cast<double>(i.u_);
  }
  if (kind_ == Kind::kInt && other.kind_ == Kind::kInt) return i_ == other.i_;
  if (kind_ == Kind::kUInt && other.kind_ == Kind::kUInt)
    return u_ == other.u_;
  // One is signed, the other unsigned.
  const int64_t si = (kind_ == Kind::kInt) ? i_ : other.i_;
  const uint64_t ui = (kind_ == Kind::kUInt) ? u_ : other.u_;
  return si >= 0 && static_cast<uint64_t>(si) == ui;
}

// ---------------------------------------------------------------- Reader ---

Reader::Reader()
    : cur_(nullptr),
      begin_(nullptr),
      end_(nullptr),
      err_(nullptr),
      allow_comments_(false),
      depth_(0),
      max_depth_(kDefaultMaxDepth) {}

Reader& Reader::allowComments() {
  allow_comments_ = true;
  return *this;
}

Reader& Reader::maxDepth(unsigned depth) {
  max_depth_ = depth;
  return *this;
}

bool Reader::parse(std::string_view str, Value& value) {
  cur_ = str.data();
  begin_ = str.data();
  end_ = str.data() + str.length();
  err_ = nullptr;
  depth_ = 0;

  JSON_CHECK_OUT_OF_RANGE;
  JSON_SKIP_WHITE_SPACE;
  if (!parseValue(value)) {
    value = nullptr;
    return false;
  }
  // After the top-level value, only whitespace (and comments, if enabled) is
  // allowed; anything else means there is trailing content that was not parsed.
  while (cur_ != end_) {
    const char c = *cur_;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      ++cur_;
    } else if (c == '/' && allow_comments_) {
      if (!skipComment()) {
        value = nullptr;
        return false;
      }
    } else {
      value = nullptr;
      return error("unexpected trailing content");
    }
  }
  return true;
}

bool Reader::parseFile(std::string_view filename, Value& value) {
  std::string content;
  if (!ReadFile(filename, content)) return false;
  // Skip a leading UTF-8 BOM.
  if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
      static_cast<unsigned char>(content[1]) == 0xBB &&
      static_cast<unsigned char>(content[2]) == 0xBF) {
    content.erase(0, 3);
  }

  return parse(content, value);
}

std::string Reader::getError() const {
  if (err_ == nullptr) return {};
  std::string err(err_);
  err += " in line ";
  unsigned line = 1;
  // count the number of rows from begin_ to cur_
  for (const char* cur = begin_; cur != cur_; ++cur) {
    if (*cur == '\n') ++line;
  }
  char buffer[21]{0};
  std::to_chars(buffer, buffer + sizeof buffer, line);
  err += buffer;
  return err;
}

bool Reader::parseValue(Value& value) {
  JSON_CHECK_OUT_OF_RANGE;
  switch (*cur_) {
    case 'n':
      return parseNull(value);
    case 't':
      return parseTrue(value);
    case 'f':
      return parseFalse(value);
    case '[':
      return parseArray(value);
    case '{':
      return parseObject(value);
    case '"':
      return parseString(value);
    default:
      return parseNumber(value);
  }
}

// Matches a literal at cur_ and consumes it.
bool Reader::matchLiteral(const char* literal, size_t length) {
  if (static_cast<size_t>(end_ - cur_) < length) return false;
  if (std::memcmp(cur_, literal, length) != 0) return false;
  cur_ += length;
  return true;
}

bool Reader::parseNull(Value& value) {
  if (!matchLiteral("null", 4)) return error("Missing 'null'");
  value = nullptr;
  return true;
}

bool Reader::parseTrue(Value& value) {
  if (!matchLiteral("true", 4)) return error("Missing 'true'");
  value = true;
  return true;
}

bool Reader::parseFalse(Value& value) {
  if (!matchLiteral("false", 5)) return error("Missing 'false'");
  value = false;
  return true;
}

bool Reader::parseString(Value& value) {
  std::string str;
  bool result = parseString(str);
  if (result) value = str;
  return result;
}

bool Reader::parseString(std::string& s) {
  // cur_ points at the opening quote.
  ++cur_;
  while (true) {
    if (cur_ >= end_) return error("missing '\"'");
    const char c = *cur_;
    switch (c) {
      case '"':
        ++cur_;
        return true;
      case '\\': {
        if (cur_ + 1 >= end_) return error("invalid escape");
        ++cur_;
        switch (*cur_) {
          case '"':
            s += '"';
            break;
          case 'n':
            s += '\n';
            break;
          case 'r':
            s += '\r';
            break;
          case 't':
            s += '\t';
            break;
          case 'f':
            s += '\f';
            break;
          case 'b':
            s += '\b';
            break;
          case '/':
            s += '/';
            break;
          case '\\':
            s += '\\';
            break;
          case 'u': {
            unsigned u = 0;
            if (!parseHex4(u)) return false;
            if (u >= 0xD800 && u <= 0xDBFF) {
              // High surrogate: must be followed by a low surrogate "\uXXXX".
              // "x\uD83D\uDE00xx"
              //        ^
              if (end_ - cur_ < 3 || cur_[1] != '\\' || cur_[2] != 'u')
                return error("missing surrogate pair");
              cur_ += 2;
              if (end_ - cur_ < 4) return error("invalid surrogate pair");
              unsigned low = 0;
              // parseHex4 returns true on success; also require a valid low
              // surrogate range (0xDC00 - 0xDFFF).
              if (!parseHex4(low) || low < 0xDC00 || low > 0xDFFF)
                return error("invalid surrogate pair");
              u = 0x10000 + ((u & 0x3FF) << 10) + (low & 0x3FF);
            } else if (u >= 0xDC00 && u <= 0xDFFF) {
              // Lone low surrogate, invalid in JSON.
              return error("invalid surrogate pair");
            }
            CodePointToUTF8(s, u);
          } break;
          default:
            return error("invalid escape");
        }
        // Move behind the escape sequence.
        ++cur_;
        break;
      }
      default:
        if (static_cast<uint8_t>(c) < ' ')
          return error(
              "The ASCII code of the characters in the string must be greater "
              "than 32");
        if (static_cast<uint8_t>(c) >= 0x80) {
          // RFC 8259: the text has to be encoded in UTF-8.
          const char* next = cur_;
          unsigned codepoint = 0;
          if (!DecodeUTF8(next, end_, codepoint))
            return error("invalid UTF-8 sequence");
          s.append(cur_, static_cast<size_t>(next - cur_));
          cur_ = next;
          continue;
        }
        s += c;
        ++cur_;
        break;
    }
  }
}

bool Reader::parseHex4(unsigned& u) {
  u = 0;
  for (unsigned i = 0; i < 4; ++i) {
    // "x\u0123x"
    //    ^
    if (cur_ + 1 >= end_) return error("unexpected ending character");
    const char ch = *++cur_;
    unsigned digit = 0;
    if (ch >= '0' && ch <= '9')
      digit = static_cast<unsigned>(ch - '0');
    else if (ch >= 'a' && ch <= 'f')
      digit = static_cast<unsigned>(ch - 'a' + 10);
    else if (ch >= 'A' && ch <= 'F')
      digit = static_cast<unsigned>(ch - 'A' + 10);
    else
      return error("invalid character");
    u = (u << 4) | digit;
  }
  return true;
}

bool Reader::parseArray(Value& value) {
  if (depth_ >= max_depth_) return error("maximum nesting depth exceeded");
  DepthGuard guard(depth_);

  ++cur_;
  value.data_ = Array();
  JSON_SKIP_WHITE_SPACE;
  // empty array
  if (*cur_ == ']') return ++cur_, true;
  while (true) {
    value.asArray().push_back(nullptr);
    JSON_SKIP_WHITE_SPACE;
    if (!parseValue(value.asArray().back())) return false;
    JSON_SKIP_WHITE_SPACE;
    const char ch = *cur_;
    if (ch == ',')
      ++cur_;
    else if (ch == ']')
      return ++cur_, true;
    else
      return error("missing ',' or ']'");
  }
}

bool Reader::parseObject(Value& value) {
  if (depth_ >= max_depth_) return error("maximum nesting depth exceeded");
  DepthGuard guard(depth_);

  ++cur_;
  value.data_ = Object();
  JSON_SKIP_WHITE_SPACE;
  // empty object
  if (*cur_ == '}') return ++cur_, true;
  while (true) {
    std::string key;
    JSON_SKIP_WHITE_SPACE;
    if (*cur_ != '"') return error("missing '\"'");
    if (!parseString(key)) return false;
    JSON_SKIP_WHITE_SPACE;
    if (*cur_ != ':') return error("missing ':'");
    ++cur_;
    JSON_CHECK_OUT_OF_RANGE;
    JSON_SKIP_WHITE_SPACE;
    if (!parseValue(value.asObject().operator[](key))) return false;
    JSON_SKIP_WHITE_SPACE;
    const char ch = *cur_;
    if (ch == ',')
      ++cur_;
    else if (ch == '}')
      return ++cur_, true;
    else
      return error("missing ',' or '}'");
  }
}

// Parses a JSON number.
//
// The grammar is validated by hand, because std::from_chars follows the
// strtod rules and therefore accepts things JSON does not allow ("1.",
// "-Infinity", ...).
//
//   number = [ minus ] int [ frac ] [ exp ]
bool Reader::parseNumber(Value& value) {
  const char* p = cur_;
  if (p >= end_) return error("unexpected ending character");
  if (*p == '-') ++p;
  if (p >= end_ || *p < '0' || *p > '9') return error("invalid character");

  // int: "0" or [1-9] *DIGIT
  if (*p == '0') {
    ++p;
  } else {
    while (p < end_ && *p >= '0' && *p <= '9') ++p;
  }

  bool is_double = false;

  // frac: "." 1*DIGIT
  if (p < end_ && *p == '.') {
    is_double = true;
    ++p;
    if (p >= end_ || *p < '0' || *p > '9')
      return error("invalid number, digits expected after '.'");
    while (p < end_ && *p >= '0' && *p <= '9') ++p;
  }

  // exp: ("e" / "E") [ minus / plus ] 1*DIGIT
  if (p < end_ && (*p == 'e' || *p == 'E')) {
    is_double = true;
    ++p;
    if (p < end_ && (*p == '+' || *p == '-')) ++p;
    if (p >= end_ || *p < '0' || *p > '9')
      return error("invalid number, digits expected in exponent");
    while (p < end_ && *p >= '0' && *p <= '9') ++p;
  }

  const char* const number_end = p;

  if (!is_double) {
    // Keep integers exact. "-0" is kept as a double so that its sign
    // survives the round trip.
    const bool negative_zero =
        (number_end - cur_ == 2 && cur_[0] == '-' && cur_[1] == '0');
    if (!negative_zero) {
      int64_t i = 0;
      auto result = std::from_chars(cur_, number_end, i);
      if (result.ec == std::errc{}) {
        cur_ = number_end;
        value = i;
        return true;
      }
      if (result.ec == std::errc::result_out_of_range) {
        uint64_t u = 0;
        auto u_result = std::from_chars(cur_, number_end, u);
        if (u_result.ec == std::errc{}) {
          cur_ = number_end;
          value = u;
          return true;
        }
      }
      return error("number out of range");
    }
  }

  double num = 0.0;
  auto result = std::from_chars(cur_, number_end, num);
  if (result.ec == std::errc::result_out_of_range)
    return error("number out of range");
  if (result.ec != std::errc{}) return error("invalid number");
  cur_ = number_end;
  value = num;
  return true;
}

bool Reader::skipWhiteSpace() {
  while (true) {
    if (cur_ >= end_) return error("unexpected ending character");
    switch (*cur_) {
      case '\t':
      case '\n':
      case '\r':
      case ' ':
        ++cur_;
        continue;
      case '/':
        // skipComment() leaves cur_ on the first character behind the
        // comment, so nothing may be consumed here.
        if (!skipComment()) return false;
        continue;
      default:
        return true;
    }
  }
}

bool Reader::skipComment() {
  // If it is true, it is allowed to parse comments in the style of'//' or'/**/'
  if (!allow_comments_) return error("comments are not allowed");
  if (cur_ + 1 >= end_) return error("invalied comment style");
  const char style = *++cur_;
  if (style == '/') {
    while (true) {
      if (cur_ + 1 >= end_) return error("unexpected ending character");
      const char c = *++cur_;
      if (c == '\n') {
        ++cur_;
        return true;
      }
    }
  } else if (style == '*') {
    while (true) {
      if (cur_ + 1 >= end_) return error("missing '*/'");
      const char c = *++cur_;
      if (c != '*') continue;
      if (cur_ + 1 >= end_) return error("missing '*/'");
      if (*++cur_ == '/') {
        ++cur_;
        return true;
      }
    }
  } else {
    return error("invalied comment style");
  }
}

bool Reader::error(const char* err) {
  err_ = err;
  return false;
}

// ---------------------------------------------------------------- Writer ---

Writer::Writer() : out_(), depth_of_indentation_(0), emit_utf8_(false) {}

Writer& Writer::emit_utf8() {
  emit_utf8_ = true;
  return *this;
}

Writer& Writer::indent(std::string_view str) {
  indent_ = str;
  return *this;
}

void Writer::writeValueFormatted(const Value& value) {
  switch (value.type()) {
    case Type::kNull:
      return writeNull();
    case Type::kBoolean:
      return writeBoolean(value);
    case Type::kNumber:
      return writeNumber(value);
    case Type::kString:
      return writeString(value.asString());
    case Type::kArray:
      return writeArrayFormatted(value);
    case Type::kObject:
      return writeObjectFormatted(value);
  }
}

void Writer::writeValue(const Value& value) {
  switch (value.type()) {
    case Type::kNull:
      return writeNull();
    case Type::kBoolean:
      return writeBoolean(value);
    case Type::kNumber:
      return writeNumber(value);
    case Type::kString:
      return writeString(value.asString());
    case Type::kArray:
      return writeArray(value);
    case Type::kObject:
      return writeObject(value);
  }
}

const std::string& Writer::getOutput() const { return out_; }

void Writer::writeHex16Bit(unsigned u) {
  constexpr const char hex2[513] =
      "000102030405060708090a0b0c0d0e0f"
      "101112131415161718191a1b1c1d1e1f"
      "202122232425262728292a2b2c2d2e2f"
      "303132333435363738393a3b3c3d3e3f"
      "404142434445464748494a4b4c4d4e4f"
      "505152535455565758595a5b5c5d5e5f"
      "606162636465666768696a6b6c6d6e6f"
      "707172737475767778797a7b7c7d7e7f"
      "808182838485868788898a8b8c8d8e8f"
      "909192939495969798999a9b9c9d9e9f"
      "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
      "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
      "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
      "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
      "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
      "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
  const unsigned hi = (u >> 8) & 0xFF;
  const unsigned lo = u & 0xFF;
  out_ += hex2[2 * hi];
  out_ += hex2[2 * hi + 1];
  out_ += hex2[2 * lo];
  out_ += hex2[2 * lo + 1];
}

void Writer::writeIndent() {
  for (unsigned i = depth_of_indentation_; i; --i) out_ += indent_;
}

void Writer::writeNull() { out_.append("null", 4); }

void Writer::writeBoolean(const Value& value) {
  value.asBool() ? out_.append("true", 4) : out_.append("false", 5);
}

void Writer::writeNumber(const Value& value) {
  // NaN and Infinity have no JSON representation, they are written as null.
  out_ += value.asNumber().toString();
}

void Writer::writeString(std::string_view str) {
  const char* cur = str.data();
  const char* end = cur + str.length();
  out_ += '"';
  while (cur < end) {
    const unsigned char c = static_cast<unsigned char>(*cur);
    switch (c) {
      case '"':
        out_ += "\\\"";
        ++cur;
        continue;
      case '\\':
        out_ += "\\\\";
        ++cur;
        continue;
      case '\b':
        out_ += "\\b";
        ++cur;
        continue;
      case '\f':
        out_ += "\\f";
        ++cur;
        continue;
      case '\n':
        out_ += "\\n";
        ++cur;
        continue;
      case '\r':
        out_ += "\\r";
        ++cur;
        continue;
      case '\t':
        out_ += "\\t";
        ++cur;
        continue;
      default:
        break;
    }
    // Control characters have to be escaped.
    if (c < 0x20) {
      out_ += "\\u";
      writeHex16Bit(c);
      ++cur;
      continue;
    }
    if (c < 0x80) {
      out_ += static_cast<char>(c);
      ++cur;
      continue;
    }
    // Multi byte sequence: the text has to be valid UTF-8.
    const char* next = cur;
    unsigned codepoint = 0;
    if (DecodeUTF8(next, end, codepoint)) {
      if (emit_utf8_) {
        out_.append(cur, static_cast<size_t>(next - cur));
      } else if (codepoint < 0x10000) {
        // Basic Multilingual Plane
        out_ += "\\u";
        writeHex16Bit(codepoint);
      } else {
        // Extended Unicode. Encode 20 bits as a surrogate pair.
        codepoint -= 0x10000;
        out_ += "\\u";
        writeHex16Bit(0xD800 + ((codepoint >> 10) & 0x3FF));
        out_ += "\\u";
        writeHex16Bit(0xDC00 + (codepoint & 0x3FF));
      }
      cur = next;
      continue;
    }
    // Invalid UTF-8: substitute the replacement character.
    if (emit_utf8_) {
      out_ += "\xEF\xBF\xBD";  // U+FFFD
    } else {
      out_ += "\\u";
      writeHex16Bit(kReplacementCharacter);
    }
    ++cur;
  }
  out_ += '"';
}

void Writer::writeArray(const Value& value) {
  out_ += '[';
  if (!value.asArray().empty()) {
    for (auto& val : value.asArray()) {
      writeValue(val);
      out_ += ',';
    }
    out_.pop_back();
  }
  out_ += ']';
}

void Writer::writeObject(const Value& value) {
  out_ += '{';
  if (!value.asObject().empty()) {
    for (auto& [key, val] : value.asObject()) {
      writeString(key);
      out_ += ':';
      writeValue(val);
      out_ += ',';
    }
    out_.pop_back();
  }
  out_ += '}';
}

void Writer::writeArrayFormatted(const Value& value) {
  out_ += '[';
  if (!value.asArray().empty()) {
    out_ += '\n';
    ++depth_of_indentation_;
    for (auto& val : value.asArray()) {
      writeIndent();
      writeValueFormatted(val);
      out_ += ",\n";
    }
    --depth_of_indentation_;
    out_.pop_back();
    out_.pop_back();
    out_ += '\n';
    writeIndent();
  }
  out_ += ']';
}

void Writer::writeObjectFormatted(const Value& value) {
  out_ += '{';
  if (!value.asObject().empty()) {
    out_ += '\n';
    ++depth_of_indentation_;
    for (auto& [key, val] : value.asObject()) {
      writeIndent();
      writeString(key);
      out_ += ": ";
      writeValueFormatted(val);
      out_ += ",\n";
    }
    --depth_of_indentation_;
    out_.pop_back();
    out_.pop_back();
    out_ += '\n';
    writeIndent();
  }
  out_ += '}';
}

// ----------------------------------------------------------------- Value ---

Value::Value() : data_(nullptr) {}

Value::Value(nullptr_t) : data_(nullptr) {}

Value::Value(bool value) : data_(value) {}

Value::Value(double value) : data_(Number(value)) {}

Value::Value(int64_t value) : data_(Number(value)) {}

Value::Value(uint64_t value) : data_(Number(value)) {}

Value::Value(const char* value) : data_(value) {}

Value::Value(const std::string& value) : data_(value) {}

Value::Value(const Value& other) : data_(other.data_) {};

Value::Value(Value&& other) noexcept : data_(other.data_) {};

Value::~Value() {}

Value& Value::operator=(const Value& other) { return operator=(Value(other)); };

Value& Value::operator=(Value&& other) noexcept {
  swap(other);
  return *this;
};

bool Value::operator==(const Value& other) const {
  return data_ == other.data_;
}

Value& Value::operator[](const std::string& index) {
  JSON_ASSERT(isObject() || isNull(), "Value must be object or null");
  if (isNull()) data_ = Object();
  return asObject()[index];
}

Value& Value::operator[](size_t index) {
  JSON_ASSERT(isArray() || isNull(), "Value must be array or null");
  if (isNull()) data_ = Array();
  if (index >= asArray().size()) Fatal("index out of range");
  return asArray()[index];
}

void Value::insert(const std::string& index, Value&& value) {
  JSON_ASSERT(isObject() || isNull(), "Value must be object or null");
  if (isNull()) data_ = Object();
  asObject().emplace(index, value);
}

bool Value::asBool() const { return std::get<bool>(data_); }

int Value::asInt() const { return std::get<Number>(data_).asInt(); }

unsigned Value::asUInt() const { return std::get<Number>(data_).asUInt(); }

int64_t Value::asInt64() const { return std::get<Number>(data_).asInt64(); }

uint64_t Value::asUInt64() const { return std::get<Number>(data_).asUInt64(); }

double Value::asDouble() const { return std::get<Number>(data_).asDouble(); }

Number Value::asNumber() const { return std::get<Number>(data_); }

std::string Value::asString() const { return std::get<std::string>(data_); }

Array& Value::asArray() { return std::get<Array>(data_); }

const Array& Value::asArray() const { return std::get<Array>(data_); }

Object& Value::asObject() { return std::get<Object>(data_); }

const Object& Value::asObject() const { return std::get<Object>(data_); }

void Value::swap(Value& other) { data_.swap(other.data_); }

bool Value::remove(const std::string& index) {
  if (isObject()) {
    asObject().erase(index);
    return true;
  }
  return false;
}

bool Value::remove(size_t index) {
  if (isArray() && index < size()) {
    asArray().erase(asArray().begin() + index);
    return true;
  }
  return false;
}

void Value::append(const Value& value) { append(Value(value)); }

void Value::append(Value&& value) {
  JSON_ASSERT(isArray() || isNull(), "Value must be array or null");
  if (isNull()) data_ = Array();
  asArray().push_back(std::move(value));
}

size_t Value::size() const {
  switch (type()) {
    case Type::kString:
      return asString().size();
    case Type::kArray:
      return asArray().size();
    case Type::kObject:
      return asObject().size();
    default:
      break;
  }
  return 0;
}

bool Value::empty() const {
  switch (type()) {
    case Type::kNull:
      return true;
    case Type::kString:
      return asString().empty();
    case Type::kArray:
      return asArray().empty();
    case Type::kObject:
      return asObject().empty();
    default:
      break;
  }
  return false;
}

bool Value::contains(const std::string& key) const {
  if (isObject()) return asObject().find(key) != asObject().end();
  return false;
}

void Value::clear() {
  switch (type()) {
    case Type::kString:
      asString().clear();
      break;
    case Type::kArray:
      asArray().clear();
      break;
    case Type::kObject:
      asObject().clear();
      break;
    default:
      break;
  }
  // type_ = Type::kNull;
}

std::string Value::dump(bool emit_utf8, std::string_view indent) const {
  Writer w;
  if (emit_utf8) w.emit_utf8();
  w.indent(indent);
  w.writeValueFormatted(*this);
  return w.getOutput();
}

// Get type

constexpr Type Value::type() const { return static_cast<Type>(data_.index()); }

bool Value::isNull() const { return type() == Type::kNull; }

bool Value::isBool() const { return type() == Type::kBoolean; }

bool Value::isNumber() const { return type() == Type::kNumber; }

bool Value::isString() const { return type() == Type::kString; }

bool Value::isArray() const { return type() == Type::kArray; }

bool Value::isObject() const { return type() == Type::kObject; }

}  // namespace json

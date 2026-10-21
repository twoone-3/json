/*
Author: twoone3
Last change: 2026.10.21
Github: https://github.com/twoone-3/json
*/
#pragma once

#if defined(__clang__) || defined(__GNUC__)
#define CPP_STANDARD __cplusplus
#elif defined(_MSC_VER)
#define CPP_STANDARD _MSVC_LANG
#endif

#if CPP_STANDARD < 201703L
#error json.h require std::c++17
#endif

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace json {

class Value;

// A JSON number.
//
// Numbers are kept in the exact representation they were written in, so that
// a document survives a parse/serialize round trip without losing precision:
//
//   - kDouble : an IEEE-754 double (fractions, exponents, "-0")
//   - kInt    : a signed 64 bit integer
//   - kUInt   : an unsigned 64 bit integer (values above INT64_MAX)
//
// RFC 8259 allows implementations to limit the range and the precision of the
// numbers they accept, so values that cannot be represented at all (for
// example "1e400") are rejected by the reader.
class Number {
 public:
  enum class Kind : uint8_t { kDouble, kInt, kUInt };

  Number();
  explicit Number(double value);
  explicit Number(int64_t value);
  explicit Number(uint64_t value);

  Kind kind() const;
  // True for kInt / kUInt, false for kDouble.
  bool isInteger() const;

  // Lossy conversions. Values that do not fit are clamped instead of
  // invoking undefined behaviour.
  double asDouble() const;
  int asInt() const;
  unsigned asUInt() const;
  int64_t asInt64() const;
  uint64_t asUInt64() const;

  // The JSON representation of the number. Non finite doubles (NaN, Infinity)
  // have no JSON representation and are written as "null".
  std::string toString() const;

  bool operator==(const Number&) const;
  bool operator!=(const Number&) const;

 private:
  Kind kind_;
  double d_;
  int64_t i_;
  uint64_t u_;
};

using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;
using Data = std::variant<nullptr_t, bool, Number, std::string, Array, Object>;

// Type of the value held by a Value object.
enum class Type : uint8_t {
  kNull,     // null value
  kBoolean,  // boolean value (bool)
  kNumber,   // number value (Number)
  kString,   // UTF-8 string value (std::string)
  kArray,    // array value (Array)
  kObject    // object value (Object)
};

// JSON Reader
class Reader {
 public:
  Reader();
  Reader& allowComments();
  // Maximum nesting depth accepted by the parser (default: 512).
  Reader& maxDepth(unsigned depth);
  bool parse(std::string_view, Value&);
  bool parseFile(std::string_view, Value&);
  std::string getError() const;

 private:
  bool parseValue(Value&);
  bool parseNull(Value&);
  bool parseTrue(Value&);
  bool parseFalse(Value&);
  bool parseString(Value&);
  bool parseString(std::string&);
  bool parseHex4(unsigned&);
  bool parseArray(Value&);
  bool parseObject(Value&);
  bool parseNumber(Value&);
  bool matchLiteral(const char*, size_t);
  bool skipWhiteSpace();
  bool skipComment();
  // always return false
  bool error(const char*);

  const char* cur_;
  const char* begin_;
  const char* end_;
  const char* err_;
  bool allow_comments_;
  unsigned depth_;
  unsigned max_depth_;
};

// JSON Reader
class Writer {
 public:
  Writer();
  Writer& emit_utf8();
  Writer& indent(std::string_view str);
  void writeValueFormatted(const Value&);
  void writeValue(const Value&);
  const std::string& getOutput() const;

 private:
  void writeHex16Bit(unsigned);
  void writeIndent();
  void writeNull();
  void writeBoolean(const Value&);
  void writeNumber(const Value&);
  void writeString(std::string_view);
  void writeArray(const Value&);
  void writeObject(const Value&);
  void writeArrayFormatted(const Value&);
  void writeObjectFormatted(const Value&);

  std::string out_;
  unsigned depth_of_indentation_;
  std::string indent_;
  bool emit_utf8_;
};

// JSON Value, can be one of enum Type
class Value {
  friend class Reader;
  friend class Writer;

 public:
  Value();
  Value(nullptr_t);
  Value(bool);
  Value(double);
  Value(int64_t);
  Value(uint64_t);
  // Any other integral type (int, unsigned, long, ...) is stored exactly.
  template <typename T,
            typename = std::enable_if_t<std::is_integral_v<T> &&
                                        !std::is_same_v<T, bool>>>
  Value(T value) : data_(asInteger(value)) {}
  Value(const char*);
  Value(const std::string&);
  Value(const Value&);
  Value(Value&&) noexcept;
  ~Value();
  Value& operator=(const Value&);
  Value& operator=(Value&&) noexcept;
  bool operator==(const Value&) const;
  Value& operator[](size_t);
  Value& operator[](const std::string&);
  void insert(const std::string&, Value&&);

  bool asBool() const;
  int asInt() const;
  unsigned asUInt() const;
  int64_t asInt64() const;
  uint64_t asUInt64() const;
  double asDouble() const;
  Number asNumber() const;
  std::string asString() const;
  Array& asArray();
  const Array& asArray() const;
  Object& asObject();
  const Object& asObject() const;

  // Exchange data from other Value
  void swap(Value&);
  // Remove a key-value pair from object
  bool remove(const std::string&);
  // Remove a value from array;
  bool remove(size_t);
  // Add elements to the array
  void append(const Value&);
  // Add elements to the array
  void append(Value&&);
  // Get size
  size_t size() const;
  bool empty() const;
  // Check whether a certain key is included
  bool contains(const std::string&) const;
  // Clear contents of value
  void clear();
  // Generate a beautiful string
  std::string dump(bool emit_utf8 = false,
                   std::string_view indent = "    ") const;
  // Get type
  constexpr Type type() const;
  bool isNull() const;
  bool isBool() const;
  bool isNumber() const;
  bool isString() const;
  bool isArray() const;
  bool isObject() const;

 private:
  // Signed types keep their sign, unsigned types keep all 64 bits.
  template <typename T>
  static Number asInteger(T value) {
    if constexpr (std::is_signed_v<T>)
      return Number(static_cast<int64_t>(value));
    else
      return Number(static_cast<uint64_t>(value));
  }

  Data data_;
};
}  // namespace json

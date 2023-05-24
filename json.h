/*
Author: twoone3
Last change: 2023.5.24
Github: https://github.com/twoone-3/json
*/
#pragma once
#pragma warning(disable : 4996)

#if defined(__clang__) || defined(__GNUC__)
#define CPP_STANDARD __cplusplus
#elif defined(_MSC_VER)
#define CPP_STANDARD _MSVC_LANG
#endif

#if CPP_STANDARD < 201703L
#error json.h require std::c++17
#endif

// #include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace json {

class Value;

using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;
using Data = std::variant<nullptr_t, bool, double, std::string, Array, Object>;

// Type of the value held by a Value object.
enum class Type : uint8_t {
  kNull,     // null value
  kBoolean,  // boolean value (bool)
  kNumber,   // number value (double)
  kString,   // UTF-8 string value (std::string)
  kArray,    // array value (Array)
  kObject    // object value (Object)
};

// JSON Reader
class Reader {
 public:
  Reader();
  Reader& allowComments();
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
  bool skipWhiteSpace();
  bool skipComment();
  // always return false
  bool error(const char*);

  const char* cur_;
  const char* begin_;
  const char* end_;
  const char* err_;
  bool allow_comments_;
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
  void writeChar(const char*& cur, const char* end, char c);
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
  Data data_;
};
}  // namespace json
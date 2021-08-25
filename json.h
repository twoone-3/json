/*
Author:
	twoone3
Last change:
	2021.8.23
Github:
	https://github.com/twoone-3/json
Reference:
	https://github.com/jo-qzy/MyJson/
	https://github.com/open-source-parsers/jsoncpp
Readme:
	This library can quickly parse or generate JSON.
	We use Dom to store JSON data, Value is the only class, and all interfaces are in the Value class.

*/
#pragma once

#if defined(__clang__) || defined(__GNUC__)
#define CPP_STANDARD __cplusplus
#elif defined(_MSC_VER)
#define CPP_STANDARD _MSVC_LANG
#endif

#if CPP_STANDARD < 201703L
#error json.h require std::c++17 <charconv> <string_view>
#endif

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <map>

namespace json {

class Value;

using std::move;

using Int = int;
using UInt = unsigned int;
using Int64 = long long;
using UInt64 = unsigned long long;
using Float = float;
using Double = double;
using String = std::string;
using StringView = std::string_view;
using Array = std::vector<Value>;
using Object = std::map<String, Value>;

//Type of the value held by a Value object.
enum ValueType :uint8_t {
	kNull,		//null value
	kBoolean,	//bool value
	kInteger,	//signed integer value
	kUInteger,	//unsigned integer value
	kReal,		//double value
	kString,	//UTF-8 string value
	kArray,		//array value (ordered list)
	kObject		//object value (collection of name/value pairs).
};
//A Value object can be one of the ValueTyoe
//you can use operator[] to modify the members of a object
class Value {
public:
	class Parser {
	public:
		bool parse(StringView, Value&, bool = false);
		String getError();
	private:
		bool parseValue(Value&);
		bool parseNull(Value&);
		bool parseTrue(Value&);
		bool parseFalse(Value&);
		bool parseString(Value&);
		bool parseString(String&);
		bool parseHex4(UInt&);
		bool parseArray(Value&);
		bool parseObject(Value&);
		bool parseNumber(Value&);
		bool skipWhiteSpace();
		bool error(const char*);

		const char* cur_ = nullptr;
		const char* begin_ = nullptr;
		const char* end_ = nullptr;
		const char* err_ = "unknown";
		bool allow_comments_ = false;
	};
	class Writer {
	public:
		Writer(UInt, bool);
		void writePrettyValue(const Value&);
		void writeValue(const Value&);
		String getOutput();
	private:
		void writeIndent();
		void writeNull();
		void writeBool(const Value&);
		void writeInt(const Value&);
		void writeUInt(const Value&);
		void writeDouble(const Value&);
		void writeString(StringView);
		void writeArray(const Value&);
		void writeObject(const Value&);
		void writePrettyArray(const Value&);
		void writePrettyObject(const Value&);

		String out_;
		UInt depth_of_indentation_;
		UInt indent_count_;
		bool emit_utf8_;
	};
	union ValueData {
		bool b;
		Int i;
		UInt u;
		Int64 i64;
		UInt64 u64;
		Float f;
		Double d;
		String* s;
		Array* a;
		Object* o;

		ValueData();
		ValueData(bool);
		ValueData(Int);
		ValueData(UInt);
		ValueData(Int64);
		ValueData(UInt64);
		ValueData(Float);
		ValueData(Double);
		ValueData(StringView);
		ValueData(ValueType);
		ValueData(const ValueData&, ValueType);
		ValueData(ValueData&&);
		void assign(const ValueData& other, ValueType type);
		void operator=(ValueData&&);
		void destroy(ValueType);
		~ValueData() = default;
		ValueData& operator=(const ValueData&) = delete;
	};
	Value();
	Value(nullptr_t);
	Value(bool);
	Value(Int);
	Value(UInt);
	Value(Int64);
	Value(UInt64);
	Value(Float);
	Value(Double);
	Value(StringView);
	Value(ValueType);
	Value(const Value&);
	Value(Value&&)noexcept;
	~Value();
	Value& operator=(const Value&);
	Value& operator=(Value&&)noexcept;
	bool equal(const Value&)const;
	bool operator==(const Value&)const;
	Value& operator[](size_t);
	Value& operator[](const String&);
	void insert(const String&, Value&&);
	bool asBool()const;
	Int asInt()const;
	UInt asUInt()const;
	Int64 asInt64()const;
	UInt64 asUInt64()const;
	Float asFloat()const;
	Double asDouble()const;
	String asString()const;
	Array asArray()const;
	Object asObject()const;
	void swap(Value&);
	// remove a key-value pair from object
	bool remove(const String&);
	// remove a value from array;
	bool remove(size_t);
	// add elements to the array
	void append(const Value&);
	// add elements to the array
	void append(Value&&);
	// get size
	size_t size()const;
	bool empty()const;
	// check whether a certain key is included
	bool contains(const String&)const;
	// clear contents of value
	void clear();
	// If indent_count is equal to 0, a compact string is generated,
	// and if indent_count is greater than 0, a beautiful string is
	// generated.
	// If emit_utf8 is false, it will convert all characters to ASCII.
	String dump(UInt indent_space = 4, bool emit_utf8 = false)const;
	// get type
	ValueType type()const { return type_; }
	bool isNull()const { return type_ == kNull; }
	bool isBool()const { return type_ == kBoolean; }
	bool isNumber()const { return type_ == kInteger || type_ == kUInteger || type_ == kReal; }
	bool isString()const { return type_ == kString; }
	bool isArray()const { return type_ == kArray; }
	bool isObject()const { return type_ == kObject; }
private:
	ValueData data_;
	ValueType type_;
};
// print the value
std::ostream& operator<<(std::ostream&, const Value&);
// parse a JSON string and output errors to stderr
Value Parse(StringView str, String* err = nullptr, bool allow_comments = false);
}// namespace Json

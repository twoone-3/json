/*
Author:
	twoone3
Last change:
	2021.8.6
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
#error json.h require c++17 <charconv> <string_view>
#endif

#include <iostream>
#include <string>
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
using Array = std::vector<Value>;
using Object = std::map<String, Value>;

//Type of the value held by a Value object.
enum ValueType :uint8_t {
	NULL_T,		//'null' value
	INT_T,		//signed integer value
	UINT_T,		//unsigned integer value
	REAL_T,		//double value
	STRING_T,	//UTF-8 string value
	BOOL_T,	//bool value
	ARRAY_T,		//array value (ordered list)
	OBJECT_T		//object value (collection of name/value pairs).
};
//A Value object can be one of the ValueTyoe
class Value {
public:
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
	};
	class Parser {
	public:
		bool parse(const String&, Value&);
		bool parse(const char*, size_t, Value&);
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
		bool error(const char*);
		const char* cur_ = nullptr;
		const char* begin_ = nullptr;
		const char* end_ = nullptr;
		const char* err_ = "unknown";
	};
	class Writer {
	public:
		void write(const Value&, bool);
		String getOutput();
	private:
		void writeIndent();
		void writeNull();
		void writeBool(const Value&);
		void writeInt(const Value&);
		void writeUInt(const Value&);
		void writeDouble(const Value&);
		void writeString(const Value&);
		void writeArray(const Value&);
		void writeObject(const Value&);
		void writePrettyArray(const Value&);
		void writePrettyObject(const Value&);
		String out_;
		UInt indent_ = 0;
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
	Value(const char*);
	Value(const String&);
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
	bool isNull()const;
	bool isBool()const;
	bool isNumber()const;
	bool isString()const;
	bool isArray()const;
	bool isObject()const;
	//获取类型
	ValueType getType()const;
	//交换内容
	void swap(Value&);
	//移除对象的指定成员
	bool remove(const String&);
	//向数组追加元素
	void append(const Value&);
	//向数组追加元素
	void append(Value&&);
	//获取大小
	size_t size()const;
	//是否为空
	bool empty()const;
	//是否拥有某个键
	bool has(const String&)const;
	//清除内容
	void clear();
	//转换成紧凑的字符串
	String toShortString()const;
	//转换成格式化的字符串
	String toStyledString()const;
	//automatic conversion
	operator String()const;
private:
	ValueData data_;
	ValueType type_;
};
//io support
std::ostream& operator<<(std::ostream&, const Value&);
//parse string
Value Parse(const String&);
}// namespace Json

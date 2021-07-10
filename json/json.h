// Json.h made by twoone3
// Github: https://github.com/twoone-3/Json
#pragma once
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
	nullValue,		//'null' value
	intValue,		//signed integer value
	uintValue,		//unsigned integer value
	realValue,		//double value
	stringValue,	//UTF-8 string value
	booleanValue,	//bool value
	arrayValue,		//array value (ordered list)
	objectValue		//object value (collection of name/value pairs).
};
//To express JSON syntax error
enum ErrorCode :uint8_t {
	OK,
	INVALID_CHARACTER,
	INVALID_ESCAPE,
	MISSING_QUOTE,
	MISSING_NULL,
	MISSING_TRUE,
	MISSING_FALSE,
	MISSING_COMMAS_OR_BRACKETS,
	MISSING_COMMAS_OR_BRACES,
	MISSING_COLON,
	MISSING_SURROGATE_PAIR,
	UNEXPECTED_ENDING_CHARACTER,
};
//A Value object can be one of the ValueTyoe
class Value {
public:
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
	Value(const ValueType);
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
	//转换字符串为JSON
	ErrorCode parse(const char*, size_t len);
	//转换字符串为JSON
	ErrorCode parse(const String&);
private:
	ErrorCode _parseValue(const char*&);
	void _toString(String&, UInt&, bool)const;
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
	} data_;
	ValueType type_;
};
//io support
std::ostream& operator<<(std::ostream&, const Value&);
//io support
std::ostream& operator<<(std::ostream&, ErrorCode);

}// namespace Json

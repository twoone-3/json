/*
Author:
	twoone3
Last change:
	2021.7.11
Github:
	https://github.com/twoone-3/Json
Reference:
	https://github.com/jo-qzy/MyJson/
	https://github.com/open-source-parsers/jsoncpp
Readme:
	This library can quickly parse or generate JSON.
	We use Dom to store JSON data, Value is the only class, and all interfaces are in the Value class.

*/
#pragma once
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <charconv>

#if !_HAS_CXX17
#error json.h require c++17 <charconv> <string_view>
#endif
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
private:
	void _toString(String&, UInt&, bool)const;
	ValueData data_;
	ValueType type_;
};
//io support
std::ostream& operator<<(std::ostream&, const Value&);

}// namespace Json

// Json.h made by twoone3
// Github: https://github.com/twoone-3/Json
#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>

namespace json {

class Value;
class Reader;
class Writer;

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
//A Value object can be one of the ValueTyoe
class Value {
public:
	friend class Reader;
	friend class Writer;
	Value();
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
	String toFastString()const;
	//转换成格式化的字符串
	String toStyledString()const;
	//转换字符串为JSON
	bool parse(const char*);
	//转换字符串为JSON
	bool parse(const String&);
private:
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
//用于解析JSON
class Reader {
public:
	Reader(const char*);

	String getErrorString();
	bool readValue(Value&);
private:
	const char& readChar();
	const char& readNextCharFront();
	const char& readNextCharBack();
	void nextChar();
	bool readNull();
	bool readTrue(Value&);
	bool readFalse(Value&);
	bool readNumber(Value&);
	bool readString(String&);
	bool readString(Value&);
	bool readArray(Value&);
	bool readObject(Value&);
	bool readWhitespace();
	bool readHex4(unsigned& u);
	bool readUnicode(unsigned& u);

	const char* ptr_;
	const char* start_;
	const char* err_;
};
//用于生成JSON字符串
class Writer {
public:
	Writer();
	String getString()const;
	void writeNumber(double);
	void writeString(const String&);
	void writeArray(const Value&);
	void writeObject(const Value&);
	void writeValue(const Value&, bool = false);
	void writeStyledArray(const Value&);
	void writeStyledObject(const Value&);
	void writeIndent();
	void writeNewline();
private:
	String out_;
	size_t indent_;
};
//io support
std::ostream& operator<<(std::ostream&, const Value&);

}// namespace Json

// Json.h made by twoone3
// Github: https://github.com/twoone-3/Json
#pragma once
#include <iostream>
#include <string>
#include <map>
#define JSON_CHECK(x) if(!x)return false

namespace Json {

using std::move;
using std::string;
//数组索引
using ArrayIndex = unsigned long long;
//对象类型
using Object = std::map<class Index, class Value>;

//JSON类型枚举
enum ValueType : char {
	null_value,
	false_value,
	true_value,
	integer_value,
	double_value,
	string_value,
	array_value,
	object_value
};
//索引类型
//当str_为0时，num_为数组索引
//当str_不为0时，str_为对象的键
class Index {
public:
	Index();
	Index(unsigned num);
	Index(ArrayIndex num);
	Index(const char* str);
	Index(const string& str);
	Index(const Index& other);
	Index(Index&& other);
	~Index();

	bool isString()const;
	string& asString()const;
	ArrayIndex asArrayIndex()const;
	Index& operator=(const Index& other);
	Index& operator=(Index&& other);
	bool operator<(const Index& other)const;
	bool operator==(const Index& other)const;
private:
	ArrayIndex num_ = 0;
	string* str_ = nullptr;
};
//联合
union Data {
	long long l;
	double d;
	string* s;
	Object* o;
};
//JSON数据类型
class Value {
public:
	Value();
	Value(const bool);
	Value(const int);
	Value(const long long);
	Value(const double);
	Value(const char*);
	Value(const string&);
	Value(const ValueType);
	Value(const Value&);
	Value(Value&&);
	~Value();
	Value& operator=(const Value&);
	Value& operator=(Value&&);
	//比较
	bool compare(const Value&)const;
	bool operator==(const Value&)const;
	//取键对应值
	Value& operator[](const int);
	Value& operator[](const ArrayIndex);
	Value& operator[](const string&);
	//插入一个值
	void insert(const string&, Value&&);
	bool asBool()const;
	int asInt()const;
	long long asInt64()const;
	double asDouble()const;
	string asString()const;
	bool isNull()const;
	bool isBool()const;
	bool isNumber()const;
	bool isString()const;
	bool isArray()const;
	bool isObject()const;
	//新建一个字符串
	void setString();
	//新建一个数组
	void setArray();
	//新建一个对象
	void setObject();
	//获取类型
	ValueType type()const;
	//获取内容
	Data& data();
	//获取内容
	const Data& data()const;
	//交换内容
	void swap(Value&);
	//移除对象的指定成员
	bool remove(const Index&);
	//向数组追加元素
	void append(const Value&);
	//向数组追加元素
	void append(Value&&);
	//获取大小
	size_t size()const;
	//是否为空
	bool empty()const;
	//是否拥有某个键
	bool has(const string&)const;
	//清除内容
	void clear();
	//返回开始的迭代器
	Object::iterator begin();
	//返回开始的迭代器
	Object::const_iterator begin()const;
	//返回结束的迭代器
	Object::iterator end();
	//返回结束的迭代器
	Object::const_iterator end()const;
	//转换成紧凑的字符串
	string toFastString()const;
	//转换成格式化的字符串
	string toStyledString()const;
private:
	Data data_;
	ValueType type_ = null_value;
};
//用于解析JSON
class Reader {
public:
	Reader();

	string getErrorString();
	bool parse(const string&, Value&);
	bool parse(const char*, Value&);
private:
	const char& readChar();
	const char& readNextCharFront();
	const char& readNextCharBack();
	void nextChar();
	bool readNull();
	bool readTrue(Value& value);
	bool readFalse(Value& value);
	bool readNumber(Value& value);
	bool readString(string& s);
	bool readString(Value& value);
	bool readArray(Value& value);
	bool readObject(Value& value);
	bool readValue(Value& value);
	bool skipBlank();
	bool readHex4(unsigned& u);
	static void encode_utf8(unsigned& u, string& s);

	const char* ptr_ = nullptr;
	const char* begin_ = nullptr;
	const char* err_ = nullptr;
};
//用于生成JSON字符串
class Writer {
public:
	const string& getString()const;
	void writeNull();
	void writeTrue();
	void writeFalse();
	void writeInteger(const long long value);
	void writeDouble(const double value);
	void writeString(const string& value);
	void writeArray(const Value& value);
	void writeObject(const Value& value);
	void writeValue(const Value& value);
	void writeIndent();
	void writeNewline();
	void writeStyledArray(const Value& value);
	void writeStyledObject(const Value& value);
	void writeStyledValue(const Value& value);
private:
	string out_;
	size_t indent_ = 0;
};


std::ostream& operator<<(std::ostream&, const Value&);

}// namespace Json
//字符串转JSON
Json::Value toJson(const char*);
Json::Value toJson(const std::string&);

/*
Author:
	twoone3
Last change:
	2021.9.21
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

// Content of indentation, tab or four spaces is recommended.
#define JSON_INDENT "    "

#if defined(__clang__) || defined(__GNUC__)
#define CXX_STANDARD __cplusplus
#elif defined(_MSC_VER)
#define CXX_STANDARD _MSVC_LANG
#endif

#if CXX_STANDARD < 201703L
#error json.h require std::c++17 <charconv> <string_view>
#endif

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <map>

namespace json {

class Value;

using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

//Type of the value held by a Value object.
enum class Type :uint8_t {
	kNull,		//null value
	kBoolean,	//bool value
	kNumber,	//number value
	kString,	//UTF-8 string value
	kArray,		//array value (ordered list)
	kObject		//object value (collection of name/value pairs).
};
//JSON Parser
class Parser {
public:
	Parser();
	Parser& allowComments();
	bool parse(std::string_view, Value&);
	std::string getError();
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
	bool error(const char*);

	const char* cur_;
	const char* begin_;
	const char* end_;
	const char* err_;
	bool allow_comments_;
};
//JSON Reader
class Writer {
public:
	Writer();
	Writer& emit_utf8();
	void writePrettyValue(const Value&);
	void writeValue(const Value&);
	const std::string& getOutput()const;
private:
	void writeIndent();
	void writeNull();
	void writeBoolean(const Value&);
	void writeNumber(const Value&);
	void writeString(std::string_view);
	void writeArray(const Value&);
	void writeObject(const Value&);
	void writePrettyArray(const Value&);
	void writePrettyObject(const Value&);

	std::string out_;
	unsigned depth_of_indentation_;
	bool emit_utf8_;
};
//JSON Value, can be one of the Type
class Value {
	friend class Parser;
	friend class Writer;
public:
	//UNION Data
	union Data {
		bool b;
		double n;
		std::string* s;
		Array* a;
		Object* o;

		Data();
		Data(bool);
		Data(int);
		Data(unsigned);
		Data(int64_t);
		Data(uint64_t);
		Data(double);
		Data(const char*);
		Data(const std::string&);
		Data(const Array&);
		Data(const Object&);
		Data(const Type);
		Data(const Data&);
		Data(Data&&);
		void operator=(const Data&);
		void operator=(Data&&);
		void destroy(Type);
		~Data() = default;
	};
	Value();
	Value(nullptr_t);
	Value(bool);
	Value(int);
	Value(unsigned);
	Value(int64_t);
	Value(uint64_t);
	Value(double);
	Value(const char*);
	Value(const std::string&);
	Value(Type);
	Value(const Value&);
	Value(Value&&)noexcept;
	~Value();
	Value& operator=(const Value&);
	Value& operator=(Value&&)noexcept;
	bool equal(const Value&)const;
	bool operator==(const Value&)const;
	Value& operator[](size_t);
	Value& operator[](const std::string&);
	void insert(const std::string&, Value&&);

	bool asBool()const;
	int asInt()const;
	unsigned asUInt()const;
	int64_t asInt64()const;
	uint64_t asUInt64()const;
	double asDouble()const;
	const std::string& asString()const;
	const Array& asArray()const;
	const Object& asObject()const;
	//Exchange data from other Value
	void swap(Value&);
	//Remove a key-value pair from object
	bool remove(const std::string&);
	//Remove a value from array;
	bool remove(size_t);
	//Add elements to the array
	void append(const Value&);
	//Add elements to the array
	void append(Value&&);
	//Get size
	size_t size()const;
	bool empty()const;
	//Check whether a certain key is included
	bool contains(const std::string&)const;
	//Clear contents of value
	void clear();
	//Generate a beautiful string
	std::string dump()const;
	//Get type
	Type type()const { return type_; }
	bool isNull()const { return type_ == Type::kNull; }
	bool isBool()const { return type_ == Type::kBoolean; }
	bool isNumber()const { return type_ == Type::kNumber; }
	bool isString()const { return type_ == Type::kString; }
	bool isArray()const { return type_ == Type::kArray; }
	bool isObject()const { return type_ == Type::kObject; }
private:
	Data data_;
	Type type_;
};
//Print the value
inline std::ostream& operator<<(std::ostream& os, const Value& value) {
	return os << value.dump();
}
}// namespace Json

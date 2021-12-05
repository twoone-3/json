/*
Author:
	twoone3
Last change:
	2021.12.5
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
constexpr auto JSON_INDENT = "    ";

#if defined(__clang__) || defined(__GNUC__)
#define CPP_STANDARD __cplusplus
#elif defined(_MSC_VER)
#define CPP_STANDARD _MSVC_LANG
#endif

#if CPP_STANDARD < 201703L
#error json.h require std::c++17
#endif

#include <iostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <map>

namespace json {

class Value;

using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;
using Data = std::variant<
	nullptr_t,
	bool,
	double,
	std::string,
	Array,
	Object
>;
//Type of the value held by a Value object.
enum class Type :uint8_t {
	kNull,		//null value
	kBoolean,	//boolean value (bool)
	kNumber,	//number value (double)
	kString,	//UTF-8 string value (std::string)
	kArray,		//array value (Array)
	kObject		//object value (Object)
};
//JSON Reader
class Reader {
public:
	Reader();
	Reader& allowComments();
	bool parse(std::string_view, Value&);
	bool parseFile(std::string_view, Value&);
	std::string getError()const;
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
	void writeValueFormatted(const Value&);
	void writeValue(const Value&);
	const std::string& getOutput()const;
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
	bool emit_utf8_;
};
//JSON Value, can be one of enum Type
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
	Value(Value&&)noexcept;
	~Value();
	Value& operator=(const Value&);
	Value& operator=(Value&&)noexcept;
	bool operator==(const Value&)const;
	Value& operator[](size_t);
	Value& operator[](const std::string&);
	void insert(const std::string&, Value&&);

	constexpr bool& asBool() { return std::get<bool>(data_); };
	//constexpr int& asInt() { return std::get<int>(data_); };
	//constexpr unsigned& asUInt() { return std::get<unsigned>(data_); };
	//constexpr int64_t& asInt64() { return std::get<int64_t>(data_); };
	//constexpr uint64_t& asUInt64() { return std::get<uint64_t>(data_); };
	constexpr double& asNumber() { return std::get<double>(data_); };
	constexpr std::string& asString() { return std::get<std::string>(data_); };
	constexpr Array& asArray() { return std::get<Array>(data_); };
	constexpr Object& asObject() { return std::get<Object>(data_); };

	constexpr const bool& asBool()const { return std::get<bool>(data_); };
	//constexpr const int& asInt()const { return std::get<int>(data_); };
	//constexpr const unsigned& asUInt()const { return std::get<unsigned>(data_); };
	//constexpr const int64_t& asInt64()const { return std::get<int64_t>(data_); };
	//constexpr const uint64_t& asUInt64()const { return std::get<uint64_t>(data_); };
	constexpr const double& asNumber()const { return std::get<double>(data_); };
	constexpr const std::string& asString()const { return std::get<std::string>(data_); };
	constexpr const Array& asArray()const { return std::get<Array>(data_); };
	constexpr const Object& asObject()const { return std::get<Object>(data_); };
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
	constexpr Type type()const { return static_cast<Type>(data_.index()); }
	bool isNull()const { return type() == Type::kNull; }
	bool isBool()const { return type() == Type::kBoolean; }
	bool isNumber()const { return type() == Type::kNumber; }
	bool isString()const { return type() == Type::kString; }
	bool isArray()const { return type() == Type::kArray; }
	bool isObject()const { return type() == Type::kObject; }
private:
	Data data_;
};
//Print the value
inline std::ostream& operator<<(std::ostream& os, const Value& value) {
	return os << value.dump();
}
}// namespace json
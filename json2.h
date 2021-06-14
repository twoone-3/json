#pragma once

#include <iostream>
#include <string>

namespace Json {

class Value;

using std::string;
using std::allocator;
using Member = std::pair<string, Value>;

static allocator<char> char_alc;
static allocator<Value> value_alc;
static allocator<Member> member_alc;

constexpr size_t DEFAULT_SIZE = 4;

enum ValueType :uint8_t {
	kNULL,
	kTRUE,
	kFALSE,
	kSTRING,
	kARRAY,
	kOBJECT,
	//number types
	kINT,
	kUINT,
	kINT64,
	kUINT64,
	kDOUBLE,
};

union Number {
	struct I {
		int i;
		char padding[4];
	} i;
	struct U {
		unsigned u;
		char padding[4];
	} ui;
	int64_t i64;
	uint64_t u64;
	double d;
};

struct String {
	size_t size;
	size_t capacity;
	char* data;
};

struct Array {
	size_t size;
	size_t capacity;
	Value* data;
};

struct Object {
	size_t size;
	size_t capacity;
	Member* data;
};

class Value {
public:
	Value() :type_(kNULL) {}
	Value(bool b) :type_(b ? kTRUE : kFALSE) {}
	Value(int i) :type_(kINT) { data_.num.i64 = i; }
	Value(unsigned u) :type_(kUINT) { data_.num.u64 = u; }
	Value(int64_t i64) :type_(kINT64) { data_.num.i64 = i64; }
	Value(uint64_t u64) :type_(kUINT64) { data_.num.u64 = u64; }
	Value(float f) :type_(kDOUBLE) { data_.num.d = f; }
	Value(double d) :type_(kDOUBLE) { data_.num.d = d; }
	Value(const char* str) :type_(kSTRING) { _str_set(str, strlen(str)); }
	Value(const char* str, size_t len) :type_(kSTRING) { _str_set(str, len); }
	Value(const string& str) :type_(kSTRING) { _str_set(str.c_str(), str.length()); }
	Value(ValueType type) :type_(type) {
		switch (type) {
		case kSTRING:
			_str_set("", 0);
		case kARRAY:
			_arr_set();
		case kOBJECT:
			_obj_set();
		}
	}
	Value(const Value& other) :type_(other.type_) {
		switch (other.type_) {
		case Json::kNULL:
			break;
		case Json::kTRUE:
			break;
		case Json::kFALSE:
			break;
		case Json::kSTRING:
			_str_set(other.data_.str.data, other.data_.str.size);
			break;
		case Json::kARRAY:
			break;
		case Json::kOBJECT:
			break;
		case Json::kINT:
			break;
		case Json::kUINT:
			break;
		case Json::kINT64:
			break;
		case Json::kUINT64:
			break;
		case Json::kDOUBLE:
			break;
		default:
			break;
		}
	}
	Value(Value&& other) {

	}
	//Generate json string
	string toString()const {
		string str;
		_accept(str);
		return str;
	}
	//Parse json string
	bool parse(const char* str) {

	}
	//
	Value& operator[](const string& key) {
		Value* value = _obj_find(key);
		if (value)
			return *value;
	}
private:
	void _accept(string& str)const {
		switch (type_) {
		case Json::kNULL:
			str.append("null", 4);
			break;
		case Json::kTRUE:
			str.append("true", 4);
			break;
		case Json::kFALSE:
			str.append("false", 5);
			break;
		case Json::kSTRING:
			str.push_back('"');
			str.append(data_.str.data, data_.str.size);
			str.push_back('"');
			break;
		case Json::kARRAY:
			str.push_back('[');
			for (size_t i = 0; i < data_.arr.size; ++i) {
				data_.arr.data[i]._accept(str);
				if (i != data_.arr.size - 1)
					str.push_back(',');
			}
			str.push_back(']');
			break;
		case Json::kOBJECT:
			str.push_back('{');
			for (size_t i = 0; i < data_.obj.size; ++i) {
				str.push_back('"');
				str.append(data_.obj.data[i].first);
				str.push_back('"');
				str.push_back(':');
				data_.obj.data[i].second._accept(str);
				if (i != data_.obj.size - 1)
					str.push_back(',');
			}
			str.push_back('}');
			break;
		case Json::kINT:
			break;
		case Json::kUINT:
			break;
		case Json::kINT64:
			break;
		case Json::kUINT64:
			break;
		case Json::kDOUBLE:
			break;
		default:
			break;
		}
	}

	Value* _obj_find(const string& key) {
		for (size_t i = 0; i < data_.obj.size; ++i) {
			if (data_.obj.data[i].first == key) {
				return &data_.obj.data[i].second;
			}
		}
		return nullptr;
	}

	void _obj_resize(size_t newsize) {
		size_t oldsize = data_.obj.capacity;
		if (oldsize > newsize)
			member_alc.deallocate(data_.obj.data + newsize, oldsize - newsize);
		else{
		Member*data=member_alc.allocate(newsize,data_.obj.data)
		}

	}
	void _str_set(const char* str, size_t len) {
		data_.str.data = char_alc.allocate(len + 1);
		memcpy(data_.str.data, str, len);
		data_.str.data[len] = '\0';
		data_.str.size = len;
	}
	void _arr_set() {
		data_.arr.data = value_alc.allocate(DEFAULT_SIZE);
		data_.arr.size = 0;
		data_.arr.capacity = DEFAULT_SIZE;
	}
	void _obj_set() {
		data_.obj.data = member_alc.allocate(DEFAULT_SIZE);
		data_.obj.size = 0;
		data_.obj.capacity = DEFAULT_SIZE;
	}
	union Data {
		Number num;
		String str;
		Array arr;
		Object obj;
	};
	ValueType type_;
	Data data_;
};

std::ostream& operator<<(std::ostream& o, const Value& value) {
	return o << value.toString();
}
}

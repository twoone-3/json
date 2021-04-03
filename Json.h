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

//JSON类型枚举
enum ValueType : char {
	null_value,
	bool_value,
	int_value,
	int64_value,
	double_value,
	string_value,
	array_value,
	object_value
};
//数组索引
using ArrayIndex = unsigned long long;
//索引类型
//当str_为0时，num_为数组索引
//当str_不为0时，str_为对象的键，num_为字符串的长度
class Index {
public:
	char* str_ = 0;
	size_t num_ = 0;

	Index() {}
	Index(const ArrayIndex n) :num_(n) {}
	Index(const char* str) {
		num_ = strlen(str);
		str_ = (char*)malloc(num_ + 1);
		memcpy(str_, str, num_ + 1);
	}
	Index(const string& str) {
		num_ = str.length();
		str_ = (char*)malloc(num_ + 1);
		memcpy(str_, str.c_str(), num_ + 1);
	}
	Index(const Index& other) {
		num_ = other.num_;
		if (other.str_) {
			str_ = (char*)malloc(num_ + 1);
			memcpy(str_, other.str_, num_ + 1);
		}
	}
	Index(Index&& other) {
		str_ = other.str_;
		num_ = other.num_;
		other.str_ = nullptr;
	}
	~Index() {
		if (str_)free(str_);
	}

	Index& operator=(const Index& other) {
		return operator=(Index(other));
	}
	Index& operator=(Index&& other) {
		std::swap(num_, other.num_);
		std::swap(str_, other.str_);
	}
	bool operator<(const Index& other)const {
		if (!str_)
			return num_ < other.num_;
		;
		int comp = memcmp(str_, other.str_, std::min(num_, other.num_));
		if (comp < 0)
			return true;
		if (comp > 0)
			return false;
		return (num_ < other.num_);
	}
	bool operator==(const Index& other)const {
		if (!str_)
			return num_ == other.num_;
		if (num_ != other.num_)
			return false;
		int comp = memcmp(str_, other.str_, num_);
		return comp == 0;
	}
};

//对象类型
using Object = std::map<Index, class Value>;
//空迭代器
static Object::iterator null_iterator;
//空迭代器
static Object::const_iterator null_const_iterator;

//JSON数据类型
class Value {
public:
	Value() : type_(null_value) {}
	Value(const bool b) : type_(bool_value) { data_.b = b; }
	Value(const int i) : type_(int_value) { data_.i = i; }
	Value(const long long l) : type_(int64_value) { data_.l = l; }
	Value(const double d) : type_(double_value) { data_.d = d; }
	Value(const char* s) : type_(string_value) { data_.s = new string(s); }
	Value(const string& s) : type_(string_value) { data_.s = new string(s); }

	Value(const ValueType type) :type_(type) {
		switch (type) {
		case string_value:data_.s = new string; break;
		case array_value:
		case object_value:data_.o = new Object; break;
		}
	};
	Value(const Value& other) {
		switch (other.type_) {
		case null_value:break;
		case bool_value:data_.b = other.data_.b; break;
		case int_value:data_.i = other.data_.i; break;
		case int64_value:data_.l = other.data_.l; break;
		case double_value:data_.d = other.data_.d; break;
		case string_value:
			data_.s = new string(*other.data_.s);
			break;
		case array_value:
		case object_value:
			data_.o = new Object(*other.data_.o);
			break;
		}
		type_ = other.type_;
	};
	Value(Value&& other) {
		data_ = other.data_;
		type_ = other.type_;
		other.type_ = null_value;
	};

	~Value() { clear(); }

	Value& operator=(const Value& other) {
		return operator=(Value(other));
	};
	Value& operator=(Value&& other)noexcept {
		std::swap(data_, other.data_);
		std::swap(type_, other.type_);
		return *this;
	};

	//比较
	bool compare(const Value& other)const {
		if (type_ != other.type_)
			return false;
		switch (other.type_) {
		case null_value:break;
		case bool_value:return data_.b == other.data_.b;
		case int_value:return data_.i == other.data_.i;
		case int64_value:return data_.l == other.data_.l;
		case double_value:return data_.d == other.data_.d;
		case string_value:return *data_.s == *other.data_.s;
		case array_value:
		case object_value:return *data_.o == *other.data_.o;
		}
		return true;
	}
	bool operator==(const Value& other)const {
		return compare(other);
	}

	//取键对应值
	Value& operator[](const Index& index) {
		if (index.str_)
			setObject();
		else
			setArray();
		return data_.o->operator[](index);
	}
	//插入一个值
	void insert(const Index& index, Value&& value) {
		if (index.str_)
			setObject();
		else
			setArray();
		data_.o->insert_or_assign(index, value);
	}

	//强转类型无检查!
	//*(T*)this
	template<typename T>
	T& as() { return *(T*)this; }

	bool& asBool() { return data_.b; }
	int& asInt() { return data_.i; }
	long long& asInt64() { return data_.l; }
	double& asDouble() { return data_.d; }
	string& asString() { return *data_.s; }

	const bool& asBool()const { return data_.b; }
	const int& asInt()const { return data_.i; }
	const long long& asInt64()const { return data_.l; }
	const double& asDouble()const { return data_.d; }
	const string& asString()const { return *data_.s; }

	bool isNull()const { return type_ == null_value; }
	bool isBool()const { return type_ == bool_value; }
	bool isInt()const { return type_ == int_value; }
	bool isInt64()const { return type_ == int64_value; }
	bool isDouble()const { return type_ == double_value; }
	bool isNumber()const { return type_ == int_value || type_ == int64_value || type_ == double_value; }
	bool isString()const { return type_ == string_value; }
	bool isArray()const { return type_ == array_value; }
	bool isObject()const { return type_ == object_value; }

	//新建一个字符串
	void setString() {
		if (type_ != string_value) {
			clear();
			data_.s = new string;
			type_ = string_value;
		}
	}
	//新建一个数组
	void setArray() {
		if (type_ != array_value) {
			clear();
			data_.o = new Object;
			type_ = array_value;
		}
	}
	//新建一个对象
	void setObject() {
		if (type_ != object_value) {
			clear();
			data_.o = new Object;
			type_ = object_value;
		}
	}

	//获取类型
	ValueType type()const { return type_; }

	//交换内容
	void swap(Value& other) {
		std::swap(data_, other.data_);
		std::swap(type_, other.type_);
	}

	//移除对象的指定成员
	bool remove(const Index& index) {
		if (isObject() && index.str_) {
			return data_.o->erase(index);
		}
		else if (isArray() && !index.str_) {
			size_t size = data_.o->size();
			if (index.num_ < size) {
				for (ArrayIndex i = index.num_; i < size - 1; ++i) {
					(*data_.o)[i].swap((*data_.o)[i + 1]);
				}
				return data_.o->erase(size - 1);
			}
		}
		return false;
	}

	//向数组追加元素
	void append(const Value& j) {
		append(Value(j));
	}
	//向数组追加元素
	void append(Value&& j) {
		setArray();
		data_.o->emplace(data_.o->size(), j);
	}

	//获取大小
	size_t size()const {
		switch (type_) {
		case null_value:return 0;
		case string_value:return data_.s->size();
		case array_value:
		case object_value:return data_.o->size();
		}
		return 1;
	}

	//是否为空
	bool empty()const {
		switch (type_) {
		case null_value:return true;
		case string_value:return data_.s->empty();
		case array_value:
		case object_value:return data_.o->empty();
		}
		return false;
	}

	//是否拥有某个键
	bool has(const string& key)const {
		if (type_ != object_value)
			return false;
		return data_.o->find(key) != data_.o->end();
	}

	//清除内容
	void clear() {
		switch (type_) {
		case string_value:delete data_.s; break;
		case array_value:
		case object_value:delete data_.o; break;
		}
		type_ = null_value;
	}

	//返回开始的迭代器
	Object::iterator begin() {
		if (isArray() || isObject())
			return data_.o->begin();
		else
			return null_iterator;
	}
	//返回开始的迭代器
	Object::const_iterator begin()const {
		if (isArray() || isObject())
			return data_.o->begin();
		else
			return null_const_iterator;
	}

	//返回结束的迭代器
	Object::iterator end() {
		if (isArray() || isObject())
			return data_.o->end();
		else
			return null_iterator;
	}
	//返回结束的迭代器
	Object::const_iterator end()const {
		if (isArray() || isObject())
			return data_.o->end();
		else
			return null_const_iterator;
	}

	//转换成紧凑的字符串
	string toString()const;

	//转换成格式化的字符串
	string toStyledString()const;

private:
	union Data {
		bool b;
		int i;
		long long l;
		double d;
		string* s;
		Object* o;
	} data_;
	ValueType type_ = null_value;
};
//用于解析JSON
class Reader {
	const char* ptr_;
	unsigned line_;
	string err_;
public:
	Reader(const char* s) :ptr_(s), line_(1) {}

	string& getErrorString() { return err_; }
	const char& readChar() { return *ptr_; }
	const char& readNextCharFront() { return *++ptr_; }
	const char& readNextCharBack() { return *ptr_++; }
	void nextChar() { ++ptr_; }
	void nextLine() { ++line_; }
	void addError(const char* s) {
		err_.append(s);
		err_.append(" at Line ");
		err_.append(std::to_string(line_));
	}

	bool readNull() {
		if (readNextCharFront() == 'u' && readNextCharFront() == 'l' && readNextCharFront() == 'l') {
			nextChar();
			return true;
		}
		else {
			addError("Miss 'null'");
			return false;
		}
	}
	bool readTrue(Value& value) {
		if (readNextCharFront() == 'r' && readNextCharFront() == 'u' && readNextCharFront() == 'e') {
			nextChar();
			value = true;
			return true;
		}
		else {
			addError("Miss 'true'");
			return false;
		}
	}
	bool readFalse(Value& value) {
		if (readNextCharFront() == 'a' && readNextCharFront() == 'l' && readNextCharFront() == 's' && readNextCharFront() == 'e') {
			nextChar();
			value = false;
			return true;
		}
		else {
			addError("Miss 'false'");
			return false;
		}
	}
	bool readNumber(Value& value) {
		char* end;
		double num = strtod(ptr_, &end);
		bool isdouble = false;
		for (const char* tmp = ptr_; tmp != end; tmp++) {
			switch (*tmp) {
			case'.':
			case'e':
			case'E':
				isdouble = true;
			}
		}
		if (isdouble)
			value = num;
		else
			value = (int)num;
		ptr_ = end;
		return true;
	}
	bool readstring(string& s) {
		nextChar();
		char ch;
		while (true) {
			ch = readNextCharBack();
			switch (ch) {
			case'\0':
				addError("Miss '\"'");
				return false;
			case '\"':
				return true;
			case '\\':
				switch (readNextCharBack()) {
				case '\"': s += '\"'; break;
				case 'n': s += '\n'; break;
				case 'r': s += '\r'; break;
				case 't': s += '\t'; break;
				case 'f': s += '\f'; break;
				case 'b': s += '\b'; break;
				case '/': s += '/'; break;
				case '\\': s += '\\'; break;
				case 'u':
				{
					unsigned u = 0;
					readHex4(u);
					if (u >= 0xD800 && u <= 0xDBFF) {
						if (readNextCharBack() != '\\') {
							addError("Invalid character");
							return false;
						}
						if (readNextCharBack() != 'u') {
							addError("Invalid character");
							return false;
						}
						unsigned tmp_u;
						readHex4(tmp_u);
						if (tmp_u < 0xDC00 || tmp_u > 0xDFFF) {
							addError("Invalid character");
							return false;
						}
						u = 0x10000 + (u - 0xD800) * 0x400 + (tmp_u - 0xDC00);
					}
					if (u > 0x10FFFF) {
						addError("Invalid character");
						return false;
					}
					encode_utf8(u, s);
					break;
				}
				default:
					addError("Invalid Escape character");
					return false;
				}
				break;
			default:
				s.push_back(ch);
				break;
			}
		}
		return true;
	}
	bool readstring(Value& value) {
		value.setString();
		return readstring(value.asString());
	}
	bool readArray(Value& value) {
		nextChar();
		JSON_CHECK(skipBlank());
		value.setArray();
		if (readChar() == ']') {
			nextChar();
			return true;
		}
		while (true) {
			JSON_CHECK(readValue(value[value.size()]));
			JSON_CHECK(skipBlank());
			if (readChar() == ',') {
				nextChar();
			}
			else if (readChar() == ']') {
				nextChar();
				break;
			}
			else {
				addError("Miss ',' or ']'");
				return false;
			}
		}
		return true;
	}
	bool readObject(Value& value) {
		nextChar();
		JSON_CHECK(skipBlank());
		value.setObject();
		if (readChar() == '}') {
			nextChar();
			return true;
		}
		while (true) {
			JSON_CHECK(skipBlank());
			if (readChar() != '"') {
				addError("Miss '\"'");
				return false;
			}
			string key;
			JSON_CHECK(readstring(key));
			JSON_CHECK(skipBlank());
			if (readChar() != ':') {
				addError("Miss ':'");
				return false;
			}
			nextChar();
			JSON_CHECK(readValue(value[key]));
			JSON_CHECK(skipBlank());
			if (readChar() == ',')
				nextChar();
			else if (readChar() == '}') {
				nextChar();
				return true;
			}
			else {
				addError("Miss ',' or '}'");
				return false;
			}
		}
	}
	bool readValue(Value& value) {
		JSON_CHECK(skipBlank());
		switch (readChar()) {
		case '\0':
			addError("Invalid character");
			return false;
		case 'n':return readNull();
		case 't':return readTrue(value);
		case 'f':return readFalse(value);
		case '[':return readArray(value);
		case '{':return readObject(value);
		case '"':return readstring(value);
		default:return readNumber(value);
		}
		return true;
	}

	bool skipBlank() {
		while (true) {
			switch (readChar()) {
			case'\t':break;
			case'\n':nextLine(); break;
			case'\r':break;
			case' ':break;
			case'/':
				nextChar();
				//单行注释
				if (readChar() == '/') {
					nextChar();
					while (readChar() != '\n') {
						nextChar();
					}
					nextLine();
				}
				//多行注释
				else if (readChar() == '*') {
					nextChar();
					while (readChar() != '*' || *(ptr_ + 1) != '/') {
						if (readChar() == '\0') {
							addError("Miss '*/'");
							return false;
						}
						if (readChar() == '\n')
							nextLine();
						nextChar();
					}
					nextChar();
				}
				else {
					addError("Invalid comment");
					return false;
				}
				break;
			default:
				return true;
			}
			nextChar();
		}
		return true;
	}
	bool readHex4(unsigned& u) {
		u = 0;
		char ch = 0;
		for (int i = 0; i < 4; i++) {
			u <<= 4;
			ch = readNextCharBack();
			if (ch >= '0' && ch <= '9')
				u |= ch - '0';
			else if (ch >= 'a' && ch <= 'f')
				u |= ch - 'a' + 10;
			else if (ch >= 'A' && ch <= 'F')
				u |= ch - 'A' + 10;
			else {
				addError("Invalid charactor");
				return false;
			}
		}
		return true;
	}
	static void encode_utf8(unsigned& u, string& s) {
		if (u <= 0x7F)
			s += (u & 0xFF);
		else if (u <= 0x7FF) {
			s += (0xC0 | (0xFF & (u >> 6)));
			s += (0x80 | (0x3F & u));
		}
		else if (u <= 0xFFFF) {
			s += (0xE0 | (0xFF & (u >> 12)));
			s += (0x80 | (0x3F & (u >> 6)));
			s += (0x80 | (0x3F & u));
		}
		else {
			s += (0xF0 | (0xFF & (u >> 18)));
			s += (0x80 | (0x3F & (u >> 12)));
			s += (0x80 | (0x3F & (u >> 6)));
			s += (0x80 | (0x3F & u));
		}
	}
};
//用于生成JSON字符串
class Writer {
	string out_;
	size_t indent_ = 0;
public:
	string& getString() { return out_; }
	void writeNull() {
		out_.append("null", 4);
	}
	void writeBool(const bool value) {
		if (value)
			out_.append("true", 4);
		else
			out_.append("false", 5);
	}
	void writeInt(const int value) {
		out_.append(std::to_string(value));
	}
	void writeInt64(const long long value) {
		out_.append(std::to_string(value));
	}
	void writeDouble(const double value) {
		char buf[16];
		snprintf(buf, 15, "%lf", value);
		string s = buf;
		while (s.back() == '0') {
			if (*(&s.back() - 1) != '.')
				s.pop_back();
			else break;
		}
		out_.append(s);
	}
	void writestring(const string& value) {
		out_.push_back('"');
		out_.append(value);
		out_.push_back('"');
	}
	void writeArray(const Value& value) {
		out_.push_back('[');
		if (!value.empty()) {
			for (auto& i : value) {
				writeValue(i.second);
				out_.push_back(',');
			}
			out_.pop_back();
		}
		out_.push_back(']');
	}
	void writeObject(const Value& value) {
		out_.push_back('{');
		if (!value.empty()) {
			for (auto& i : value) {
				out_.push_back('"');
				out_.append(i.first.str_, i.first.num_);
				out_.push_back('"');
				out_.push_back(':');
				writeValue(i.second);
				out_.push_back(',');
			}
			out_.pop_back();
		}
		out_.push_back('}');
	}
	void writeValue(const Value& value) {
		switch (value.type()) {
		case null_value:writeNull(); break;
		case bool_value:writeBool(value.asBool()); break;
		case int_value: writeInt(value.asInt()); break;
		case int64_value: writeInt64(value.asInt64()); break;
		case double_value:writeDouble(value.asDouble()); break;
		case string_value:writestring(value.asString()); break;
		case array_value:writeArray(value); break;
		case object_value:writeObject(value); break;
		}
	}
	void writeIndent() {
		for (size_t i = indent_; i--;)
			out_.append("    ", 4);
	}
	void writeNewline() {
		out_.push_back('\n');
	}
	void writeStyledArray(const Value& value) {
		out_.push_back('[');
		if (!value.empty()) {
			writeNewline();
			++indent_;
			for (auto& i : value) {
				writeIndent();
				writeStyledValue(i.second);
				out_.push_back(',');
				writeNewline();
			}
			--indent_;
			out_.pop_back();
			out_.pop_back();
			writeNewline();
			writeIndent();
		}
		out_.push_back(']');
	}
	void writeStyledObject(const Value& value) {
		out_.push_back('{');
		if (!value.empty()) {
			writeNewline();
			++indent_;
			for (auto& i : value) {
				writeIndent();
				out_.push_back('"');
				out_.append(i.first.str_, i.first.num_);
				out_.push_back('"');
				out_.push_back(':');
				out_.push_back(' ');
				writeStyledValue(i.second);
				out_.push_back(',');
				writeNewline();
			}
			--indent_;
			out_.pop_back();
			out_.pop_back();
			writeNewline();
			writeIndent();
		}
		out_.push_back('}');
	}
	void writeStyledValue(const Value& value) {
		switch (value.type()) {
		case null_value:writeNull(); break;
		case bool_value:writeBool(value.asBool()); break;
		case int_value: writeInt(value.asInt()); break;
		case int64_value: writeInt64(value.asInt64()); break;
		case double_value:writeDouble(value.asDouble()); break;
		case string_value:writestring(value.asString()); break;
		case array_value:writeStyledArray(value); break;
		case object_value:writeStyledObject(value); break;
		}
	}
};

//封装函数-将字符串转JSON
Value toJson(const char* str) {
	Value v;
	Reader r(str);
	if (!r.readValue(v))
		std::cerr << r.getErrorString() << std::endl;
	return move(v);
}
Value toJson(const string& str) {
	return toJson(str.c_str());
}
//封装函数-将JSON转字符串
string Value::toString()const {
	Writer w;
	w.writeValue(*this);
	return w.getString();
}
//封装函数-将JSON转格式化字符串
string Value::toStyledString()const {
	Writer w;
	w.writeStyledValue(*this);
	return w.getString();
}

std::ostream& operator<<(std::ostream& o, const Value& v) {
	return o << v.toStyledString();
}
} // namespace Json

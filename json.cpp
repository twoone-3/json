#include "json.h"
#include <cassert>
#define JSON_CHECK(x) if(!x)return false
#define JSON_UTF8 0

namespace Json {
#pragma region Index
Index::Index() {}

Index::Index(unsigned num) :num_(num) {}

Index::Index(ArrayIndex num) : num_(num) {}

Index::Index(const char* str) {
	str_ = new string(str);
}

Index::Index(const string& str) {
	str_ = new string(str);
}

Index::Index(const Index& other) {
	if (other.str_) {
		str_ = new string(*other.str_);
	}
	else {
		num_ = other.num_;
	}
}

Index::Index(Index&& other) {
	num_ = other.num_;
	str_ = other.str_;
	other.str_ = nullptr;
}

Index::~Index() {
	if (str_)
		delete str_;
}

bool Index::isString()const { return str_; }

string& Index::asString()const {
	return *str_;
}

ArrayIndex Index::asArrayIndex()const { return num_; }

Index& Index::operator=(const Index& other) {
	return operator=(Index(other));
}

Index& Index::operator=(Index&& other) {
	std::swap(str_, other.str_);
	std::swap(num_, other.num_);
	return *this;
}

bool Index::operator<(const Index& other)const {
	if (str_)
		return *str_ < *other.str_;
	else
		return num_ < other.num_;
}

bool Index::operator==(const Index& other)const {
	if (str_)
		return *str_ == *other.str_;
	else
		return num_ == other.num_;
}
#pragma endregion
#pragma region Value
Value::Value() :type_(null_value) {}

Value::Value(const bool b) : type_(b ? true_value : false_value) {}

Value::Value(const int num) : type_(integer_value) { data_.l = num; }

Value::Value(const long long num) : type_(integer_value) { data_.l = num; }

Value::Value(const double num) : type_(double_value) { data_.d = num; }

Value::Value(const char* s) : type_(string_value) { data_.s = new string(s); }

Value::Value(const string& s) : type_(string_value) { data_.s = new string( s); }

Value::Value(const ValueType type) : type_(type) {
	switch (type) {
	case string_value:data_.s = new string; break;
	case array_value:
	case object_value:data_.o = new Object; break;
	}
};

Value::Value(const Value& other) {
	switch (other.type_) {
	case integer_value:
		data_.l = other.data_.l;
		break;
	case double_value:
		data_.d = other.data_.d;
		break;
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

Value::Value(Value&& other) {
	data_ = other.data_;
	type_ = other.type_;
	other.type_ = null_value;
	other.data_.o = nullptr;
};

Value::~Value() {
	switch (type_){
	case Json::null_value:
		break;
	case Json::false_value:
		break;
	case Json::true_value:
		break;
	case Json::integer_value:
		break;
	case Json::double_value:
		break;
	case Json::string_value:
		delete data_.s;
		break;
	case Json::array_value:
	case Json::object_value:
		delete data_.o;
		break;
	default:
		break;
	}
}

Value& Value::operator=(const Value& other) {
	return operator=(Value(other));
};

Value& Value::operator=(Value&& other) {
	std::swap(data_, other.data_);
	std::swap(type_, other.type_);
	return *this;
};
//比较
bool Value::compare(const Value& other)const {
	if (type_ != other.type_)
		return false;
	switch (other.type_) {
	case integer_value:return data_.l == other.data_.l;
	case double_value:return data_.d == other.data_.d;
	case string_value:return *data_.s == *other.data_.s;
	case array_value:
	case object_value:return *data_.o == *other.data_.o;
	}
	return true;
}

bool Value::operator==(const Value& other)const {
	return compare(other);
}
//取键对应值
Value& Value::operator[](const string& index) {
	setObject();
	return data_.o->operator[](index);
}
//取键对应值
Value& Value::operator[](const ArrayIndex index) {
	setArray();
	return data_.o->operator[](index);
}
//取键对应值
Value& Value::operator[](const int index) {
	setArray();
	return data_.o->operator[](ArrayIndex(index));
}
//插入一个值
void Value::insert(const string& index, Value&& value) {
	setObject();
	data_.o->emplace(index, value);
}

bool Value::asBool()const { return type_ == true_value; }

int Value::asInt()const { return int(data_.l); }

long long Value::asInt64()const { return data_.l; }

double Value::asDouble()const { return data_.d; }

string Value::asString()const {
	switch (type_) {
	case null_value:return "null";
	case false_value:return "false";
	case true_value:return "true";
	case integer_value:
	case double_value:
		return std::to_string(data_.d);
	case string_value:return *data_.s;
	}
	return "";
}

bool Value::isNull()const { return type_ == null_value; }

bool Value::isBool()const { return type_ == false_value || type_ == true_value; }

bool Value::isNumber()const { return type_ == integer_value || type_ == double_value; }

bool Value::isString()const { return type_ == string_value; }

bool Value::isArray()const { return type_ == array_value; }

bool Value::isObject()const { return type_ == object_value; }
//新建一个字符串
void Value::setString() {
	if (type_ != string_value) {
		this->~Value();
		data_.s = new string;
		type_ = string_value;
	}
}
//新建一个数组
void Value::setArray() {
	if (type_ != array_value) {
		this->~Value();
		data_.o = new Object;
		type_ = array_value;
	}
}
//新建一个对象
void Value::setObject() {
	if (type_ != object_value) {
		this->~Value();
		data_.o = new Object;
		type_ = object_value;
	}
}
//获取类型
ValueType Value::type()const { return type_; }
//交换内容
void Value::swap(Value& other) {
	std::swap(data_, other.data_);
	std::swap(type_, other.type_);
}
//移除对象的指定成员
bool Value::remove(const Index& index) {
	if (isObject() && index.isString()) {
		return data_.o->erase(index);
	}
	else if (isArray() && !index.isString()) {
		size_t size = data_.o->size();
		if (index.asArrayIndex() < size) {
			for (ArrayIndex i = index.asArrayIndex(); i < size - 1; ++i) {
				(*data_.o)[i].swap((*data_.o)[i + 1]);
			}
			return data_.o->erase(size - 1);
		}
	}
	return false;
}
//向数组追加元素
void Value::append(const Value& value) {
	append(Value(value));
}
//向数组追加元素
void Value::append(Value&& value) {
	setArray();
	data_.o->emplace(data_.o->size(), value);
}
//获取大小
size_t Value::size()const {
	switch (type_) {
	case null_value:return 0;
	case string_value:return data_.s->size();
	case array_value:
	case object_value:return data_.o->size();
	}
	return 1;
}
//是否为空
bool Value::empty()const {
	switch (type_) {
	case null_value:return true;
	case string_value:return data_.s->empty();
	case array_value:
	case object_value:return data_.o->empty();
	}
	return false;
}
//是否拥有某个键
bool Value::has(const string& key)const {
	if (!isObject())
		return false;
	return data_.o->find(key) != data_.o->end();
}
//清除内容
void Value::clear() {
	switch (type_){
	case Json::null_value:
		break;
	case Json::false_value:
		break;
	case Json::true_value:
		break;
	case Json::integer_value:
		break;
	case Json::double_value:
		break;
	case Json::string_value:
		data_.s->clear();
		break;
	case Json::array_value:
	case Json::object_value:
		data_.o->clear();
		break;
	default:
		break;
	}
	type_ = null_value;
}
//返回开始的迭代器
Object::iterator Value::begin() {
	if (isArray() || isObject())
		return data_.o->begin();
	else
		return {};
}
//返回开始的迭代器
Object::const_iterator Value::begin()const {
	if (isArray() || isObject())
		return data_.o->begin();
	else
		return {};
}
//返回结束的迭代器
Object::iterator Value::end() {
	if (isArray() || isObject())
		return data_.o->end();
	else
		return {};
}
//返回结束的迭代器
Object::const_iterator Value::end()const {
	if (isArray() || isObject())
		return data_.o->end();
	else
		return {};
}
string Value::toFastString()const {
	Writer w;
	w.writeValue(*this);
	return w.getString();
}
string Value::toStyledString()const {
	Writer w;
	w.writeStyledValue(*this);
	return w.getString();
}
bool Value::parse(const char* str) {
	Json::Reader r;
	bool res =r.parse(str, *this);
	if (!res)
		std::cerr << r.getErrorString() << std::endl;
	return res;
}
bool Value::parse(const string& str) {
	return parse(str.c_str());
}
#pragma endregion
#pragma region Reader
Reader::Reader() {}

string Reader::getErrorString() {
	string err;
	unsigned line = 1;
	for (const char* begin = begin_; begin != ptr_; ++begin) {
		if (*begin == '\n')
			++line;
	}
	return err.append(err_).append(" in Line ").append(std::to_string(line));
}

bool Reader::parse(const string& str, Value& value) {
	return parse(str.c_str(), value);
}

bool Reader::parse(const char* str, Value& value) {
	ptr_ = str;
	begin_ = str;
	return readValue(value);
}

inline const char& Reader::readChar() { return *ptr_; }

inline const char& Reader::readNextCharFront() { return *++ptr_; }

inline const char& Reader::readNextCharBack() { return *ptr_++; }

inline void Reader::nextChar() { ++ptr_; }

bool Reader::readNull() {
	if (ptr_[1] == 'u' && ptr_[2] == 'l' && ptr_[3] == 'l')
		return ptr_ += 4, true;
	else
		return err_ = "Missing 'null'", false;
}

bool Reader::readTrue(Value& value) {
	if (ptr_[1] == 'r' && ptr_[2] == 'u' && ptr_[3] == 'e')
		return value = true, ptr_ += 4, true;
	else
		return err_ = "Missing 'true'", false;
}

bool Reader::readFalse(Value& value) {
	if (ptr_[1] == 'a' && ptr_[2] == 'l' && ptr_[3] == 's' && ptr_[4] == 'e')
		return value = false, ptr_ += 5, true;
	else
		return err_ = "Missing 'false'", false;
}

bool Reader::readNumber(Value& value) {
	char* end;
	double num = strtod(ptr_, &end);
	bool isdouble = false;
	for (const char* tmp = ptr_; tmp != end; ++tmp) {
		char c = *tmp;
		if (c == '.' || c == 'e' || c == 'E') {
			isdouble = true;
			break;
		}
	}
	if (isdouble)
		value = num;
	else
		value = static_cast<long long>(num);
	ptr_ = end;
	return true;
}

bool Reader::readString(string& s) {
	nextChar();
	for (;;) {
		char ch = readNextCharBack();
		if (!ch)
			return err_ = "Missing '\"'", false;
		if (ch == '"')
			return true;
		if (ch == '\\')
			switch (readNextCharBack()) {
			case '\"': s.push_back('\"'); break;
			case 'n': s.push_back('\n'); break;
			case 'r': s.push_back('\r'); break;
			case 't': s.push_back('\t'); break;
			case 'f': s.push_back('\f'); break;
			case 'b': s.push_back('\b'); break;
			case '/': s.push_back('/'); break;
			case '\\': s.push_back('\\'); break;
			case 'u': {
				unsigned u;
				JSON_CHECK(readUnicode(u));
				if (u <= 0x7F)
					s.push_back((u & 0xFF));
				else if (u <= 0x7FF) {
					s.push_back((0xC0 | (0xFF & (u >> 6))));
					s.push_back((0x80 | (0x3F & u)));
				}
				else if (u <= 0xFFFF) {
					s.push_back((0xE0 | (0xFF & (u >> 12))));
					s.push_back((0x80 | (0x3F & (u >> 6))));
					s.push_back((0x80 | (0x3F & u)));
				}
				else {
					s.push_back((0xF0 | (0xFF & (u >> 18))));
					s.push_back((0x80 | (0x3F & (u >> 12))));
					s.push_back((0x80 | (0x3F & (u >> 6))));
					s.push_back((0x80 | (0x3F & u)));
				}
			}break;
			default:
				return err_ = "Invalid Escape character", false;
			}
		else
			s.push_back(ch);
	}
	return true;
}

bool Reader::readString(Value& value) {
	value.setString();
	return readString(*value.data_.s);
}

bool Reader::readArray(Value& value) {
	nextChar();
	JSON_CHECK(readWhitespace());
	value.setArray();
	if (readChar() == ']') {
		nextChar();
		return true;
	}
	for (;;) {
		JSON_CHECK(readValue(value[value.size()]));
		JSON_CHECK(readWhitespace());
		if (readChar() == ',') {
			nextChar();
		}
		else if (readChar() == ']') {
			nextChar();
			break;
		}
		else {
			return err_ = "Missing ',' or ']'", false;
		}
	}
	return true;
}

bool Reader::readObject(Value& value) {
	nextChar();
	JSON_CHECK(readWhitespace());
	value.setObject();
	if (readChar() == '}') {
		nextChar();
		return true;
	}
	for (;;) {
		string key;
		JSON_CHECK(readWhitespace());
		if (readChar() != '"') {
			return err_ = "Missing '\"'", false;
		}
		JSON_CHECK(readString(key));
		JSON_CHECK(readWhitespace());
		if (readChar() != ':') {
			return err_ = "Missing ':'", false;
		}
		nextChar();
		JSON_CHECK(readValue(value[key]));
		JSON_CHECK(readWhitespace());
		if (readChar() == ',')
			nextChar();
		else if (readChar() == '}') {
			nextChar();
			return true;
		}
		else {
			return err_ = "Missing ',' or '}'", false;
		}
	}
}

bool Reader::readValue(Value& value) {
	JSON_CHECK(readWhitespace());
	switch (readChar()) {
	case '\0':
		return err_ = "Invalid character", false;
	case 'n':return readNull();
	case 't':return readTrue(value);
	case 'f':return readFalse(value);
	case '[':return readArray(value);
	case '{':return readObject(value);
	case '"':return readString(value);
	default:return readNumber(value);
	}
	return true;
}

bool Reader::readWhitespace() {
	for (;;) {
		switch (readChar()) {
		case'\t':break;
		case'\n':break;
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
			}
			//多行注释
			else if (readChar() == '*') {
				nextChar();
				while (readChar() != '*' || *(ptr_ + 1) != '/') {
					if (readChar() == '\0') {
						return err_ = "Missing '*/'", false;
					}
					nextChar();
				}
				nextChar();
			}
			else
				return err_ = "Invalid comment", false;
			break;
		default:
			return true;
		}
		nextChar();
	}
	return true;
}

bool Reader::readHex4(unsigned& unicode) {
	unsigned u = 0;
	char ch = 0;
	for (unsigned i = 0; i < 4; ++i) {
		u <<= 4;
		ch = readNextCharBack();
		if (ch >= '0' && ch <= '9')
			u |= ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			u |= ch - 'a' + 10;
		else if (ch >= 'A' && ch <= 'F')
			u |= ch - 'A' + 10;
		else
			return err_ = "Invalid character", false;
	}
	unicode = u;
	return true;
}

bool Reader::readUnicode(unsigned& unicode) {
	unsigned u = 0;
	readHex4(u);
	if (u >= 0xD800 && u <= 0xDBFF) {
		if (readNextCharBack() == '\\' && readNextCharBack() == 'u') {
			unsigned surrogatePair;
			if (readHex4(surrogatePair))
				u = 0x10000 + ((u & 0x3FF) << 10) + (surrogatePair & 0x3FF);
			else
				return false;
		}
		else
			return err_ = "expecting another \\u token to begin the second half of a unicode surrogate pair", false;
	}
	unicode = u;
	return true;
}
#pragma endregion
#pragma region Writer
static bool hasEsecapingChar(const string& str) {
	for (auto c : str) {
		if (c == '\\' || c == '"' || c < 0x20 || c > 0x7F)
			return true;
	}
	return false;
}

static unsigned int utf8ToCodepoint(const char* s, const char* e) {
	const unsigned int REPLACEMENT_CHARACTER = 0xFFFD;

	unsigned int firstByte = static_cast<unsigned char>(*s);

	if (firstByte < 0x80)
		return firstByte;

	if (firstByte < 0xE0) {
		if (e - s < 2)
			return REPLACEMENT_CHARACTER;

		unsigned int calculated =
			((firstByte & 0x1F) << 6) | (static_cast<unsigned int>(s[1]) & 0x3F);
		s += 1;
		// oversized encoded characters are invalid
		return calculated < 0x80 ? REPLACEMENT_CHARACTER : calculated;
	}

	if (firstByte < 0xF0) {
		if (e - s < 3)
			return REPLACEMENT_CHARACTER;

		unsigned int calculated = ((firstByte & 0x0F) << 12) |
			((static_cast<unsigned int>(s[1]) & 0x3F) << 6) |
			(static_cast<unsigned int>(s[2]) & 0x3F);
		s += 2;
		// surrogates aren't valid codepoints itself
		// shouldn't be UTF-8 encoded
		if (calculated >= 0xD800 && calculated <= 0xDFFF)
			return REPLACEMENT_CHARACTER;
		// oversized encoded characters are invalid
		return calculated < 0x800 ? REPLACEMENT_CHARACTER : calculated;
	}

	if (firstByte < 0xF8) {
		if (e - s < 4)
			return REPLACEMENT_CHARACTER;

		unsigned int calculated = ((firstByte & 0x07) << 18) |
			((static_cast<unsigned int>(s[1]) & 0x3F) << 12) |
			((static_cast<unsigned int>(s[2]) & 0x3F) << 6) |
			(static_cast<unsigned int>(s[3]) & 0x3F);
		s += 3;
		// oversized encoded characters are invalid
		return calculated < 0x10000 ? REPLACEMENT_CHARACTER : calculated;
	}

	return REPLACEMENT_CHARACTER;
}

static string toHex16Bit(unsigned int x) {
	static const char hex2[] =
		"000102030405060708090a0b0c0d0e0f"
		"101112131415161718191a1b1c1d1e1f"
		"202122232425262728292a2b2c2d2e2f"
		"303132333435363738393a3b3c3d3e3f"
		"404142434445464748494a4b4c4d4e4f"
		"505152535455565758595a5b5c5d5e5f"
		"606162636465666768696a6b6c6d6e6f"
		"707172737475767778797a7b7c7d7e7f"
		"808182838485868788898a8b8c8d8e8f"
		"909192939495969798999a9b9c9d9e9f"
		"a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
		"b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
		"c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
		"d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
		"e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
		"f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
	const unsigned int hi = (x >> 8) & 0xff;
	const unsigned int lo = x & 0xff;
	string out_(4, ' ');
	out_[0] = hex2[2 * hi];
	out_[1] = hex2[2 * hi + 1];
	out_[2] = hex2[2 * lo];
	out_[3] = hex2[2 * lo + 1];
	return out_;
}

const string& Writer::getString()const { return out_; }

void Writer::writeNull() {
	out_.append("null", 4);
}

void Writer::writeTrue() {
	out_.append("true", 4);
}

void Writer::writeFalse() {
	out_.append("false", 5);
}

void Writer::writeInteger(const long long value) {
	out_.append(std::to_string(value));
}

void Writer::writeDouble(const double value) {
	string&& s = std::to_string(value);
	while (s.back() == '0') {
		if (*(&s.back() - 1) != '.')
			s.pop_back();
		else break;
	}
	out_.append(s);
}

void Writer::writeString(const string& value) {
	out_.push_back('"');
	if (hasEsecapingChar(value))
		for (auto& c : value) {
			switch (c) {
			case '\"':
				out_.append("\\\"", 2);
				break;
			case '\\':
				out_.append("\\\\", 2);
				break;
			case '\b':
				out_.append("\\b", 2);
				break;
			case '\f':
				out_.append("\\f", 2);
				break;
			case '\n':
				out_.append("\\n", 2);
				break;
			case '\r':
				out_.append("\\r", 2);
				break;
			case '\t':
				out_.append("\\t", 2);
				break;
			default:
#if JSON_UTF8
				if (uint8_t(c) < 0x20)
					out_.append("\\u").append(toHex16Bit(c));
				else
					out_.push_back(c);
#else
				{
					unsigned codepoint = utf8ToCodepoint(&c, &*value.end()); // modifies `c`
					if (codepoint < 0x20) {
						out_.append("\\u").append(toHex16Bit(codepoint));
					}
					else if (codepoint < 0x80) {
						out_.push_back(static_cast<char>(codepoint));
					}
					else if (codepoint < 0x10000) {
						// Basic Multilingual Plane
						out_.append("\\u").append(toHex16Bit(codepoint));
					}
					else {
						// Extended Unicode. Encode 20 bits as a surrogate pair.
						codepoint -= 0x10000;
						out_.append("\\u").append(toHex16Bit(0xd800 + ((codepoint >> 10) & 0x3ff)));
						out_.append("\\u").append(toHex16Bit(0xdc00 + (codepoint & 0x3ff)));
					}
				}
#endif
				break;
			}
		}
	else
		out_.append(value);
	out_.push_back('"');
}

void Writer::writeArray(const Value& value) {
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

void Writer::writeObject(const Value& value) {
	out_.push_back('{');
	if (!value.empty()) {
		for (auto& i : value) {
			writeString(i.first.asString());
			out_.push_back(':');
			writeValue(i.second);
			out_.push_back(',');
		}
		out_.pop_back();
	}
	out_.push_back('}');
}

void Writer::writeValue(const Value& value) {
	switch (value.type()) {
	case null_value:writeNull(); break;
	case false_value:writeFalse(); break;
	case true_value:writeTrue(); break;
	case integer_value:writeInteger(value.data_.l); break;
	case double_value:writeDouble(value.data_.d); break;
	case string_value:writeString(*value.data_.s); break;
	case array_value:writeArray(value); break;
	case object_value:writeObject(value); break;
	}
}

void Writer::writeIndent() {
	size_t i = indent_;
	while (i--)
		out_.push_back('\t');
}

void Writer::writeNewline() {
	out_.push_back('\n');
}

void Writer::writeStyledArray(const Value& value) {
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

void Writer::writeStyledObject(const Value& value) {
	out_.push_back('{');
	if (!value.empty()) {
		writeNewline();
		++indent_;
		for (auto& i : value) {
			writeIndent();
			writeString(i.first.asString());
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

void Writer::writeStyledValue(const Value& value) {
	switch (value.type()) {
	case null_value:writeNull(); break;
	case false_value:writeFalse(); break;
	case true_value:writeTrue(); break;
	case integer_value:writeInteger(value.data_.l); break;
	case double_value:writeDouble(value.data_.d); break;
	case string_value:writeString(*value.data_.s); break;
	case array_value:writeStyledArray(value); break;
	case object_value:writeStyledObject(value); break;
	}
}
#pragma endregion

std::ostream& operator<<(std::ostream& o, const Value& v) {
	return o << v.toStyledString();
}

} // namespace Json

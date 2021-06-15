#include "json.h"
#include <cassert>
#define JSON_CHECK(x) if(!x)return false
#define JSON_UTF8 0

namespace json {

#pragma region Value
Value::Value() :type_(nullValue) { data_.u64 = 0; }
Value::Value(bool b) : type_(booleanValue) { data_.u64 = b; }
Value::Value(Int num) : type_(intValue) { data_.i64 = num; }
Value::Value(UInt num) : type_(uintValue) { data_.u64 = num; }
Value::Value(Int64 num) : type_(intValue) { data_.i64 = num; }
Value::Value(UInt64 num) : type_(uintValue) { data_.u64 = num; }
Value::Value(Float num) : type_(realValue) { data_.d = num; }
Value::Value(Double num) : type_(realValue) { data_.d = num; }
Value::Value(const char* s) : type_(stringValue) { data_.s = new String(s); }
Value::Value(const String& s) : type_(stringValue) { data_.s = new String(s); }
Value::Value(const ValueType type) : type_(type) {
	data_.u64 = 0;
	switch (type_) {
	case nullValue:
		break;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		data_.s = new String;
		break;
	case booleanValue:
		break;
	case arrayValue:
		data_.a = new Array;
		break;
	case objectValue:
		data_.o = new Object;
		break;
	default:
		break;
	}
};
Value::Value(const Value& other) {
	data_.u64 = 0;
	type_ = other.type_;
	switch (other.type_) {
	case nullValue:
		break;
	case intValue:
		data_.i64 = other.data_.i64;
		break;
	case uintValue:
		data_.u64 = other.data_.u64;
		break;
	case realValue:
		data_.d = other.data_.d;
		break;
	case stringValue:
		data_.s = new String(*other.data_.s);
		break;
	case booleanValue:
		data_.b = other.data_.b;
		break;
	case arrayValue:
		data_.a = new Array(*other.data_.a);
		break;
	case objectValue:
		data_.o = new Object(*other.data_.o);
		break;
	default:
		break;
	}
};
Value::Value(Value&& other)noexcept {
	data_ = other.data_;
	type_ = other.type_;
	other.type_ = nullValue;
	other.data_.u64 = 0;
};
Value::~Value() {
	switch (type_) {
	case nullValue:
		break;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		delete data_.s;
		break;
	case booleanValue:
		break;
	case arrayValue:
		delete data_.a;
		break;
	case objectValue:
		delete data_.o;
		break;
	default:
		break;
	}
	type_ = nullValue;
}
Value& Value::operator=(const Value& other) {
	return operator=(Value(other));
};
Value& Value::operator=(Value&& other)noexcept {
	swap(other);
	return *this;
};
bool Value::equal(const Value& other)const {
	if (type_ != other.type_)
		return false;
	switch (type_) {
	case nullValue:
		return true;
	case intValue:
		return data_.i64 == other.data_.i64;
	case uintValue:
		return data_.u64 == other.data_.u64;
	case realValue:
		return data_.d == other.data_.d;
	case stringValue:
		return *data_.s == *other.data_.s;
	case booleanValue:
		return data_.b == other.data_.b;
	case arrayValue:
		return *data_.a == *other.data_.a;
	case objectValue:
		return *data_.o == *other.data_.o;
	default:
		break;
	}
	return false;
}
bool Value::operator==(const Value& other)const {
	return equal(other);
}
Value& Value::operator[](const String& index) {
	assert(isObject() || isNull());
	if (isNull())
		new (this)Value(objectValue);
	return data_.o->operator[](index);
}
Value& Value::operator[](size_t index) {
	assert(isArray() || isNull());
	if (isNull())
		new (this)Value(arrayValue);
	return data_.a->operator[](index);
}
void Value::insert(const String& index, Value&& value) {
	assert(isObject() || isNull());
	if (isNull())
		new (this)Value(objectValue);
	data_.o->emplace(index, value);
}
bool Value::asBool()const { return data_.b; }
Int Value::asInt()const { return data_.i; }
UInt Value::asUInt()const { return data_.u; }
Int64 Value::asInt64()const { return data_.i64; }
UInt64 Value::asUInt64()const { return data_.u64; }
Float Value::asFloat()const { return data_.f; }
Double Value::asDouble()const { return data_.d; }
String Value::asString()const {
	return *data_.s;
}
Array Value::asArray()const {
	return *data_.a;
}
Object Value::asObject()const {
	return *data_.o;
}
bool Value::isNull()const { return type_ == nullValue; }
bool Value::isBool()const { return type_ == booleanValue; }
bool Value::isNumber()const { return type_ == intValue || type_ == uintValue || type_ == realValue; }
bool Value::isString()const { return type_ == stringValue; }
bool Value::isArray()const { return type_ == arrayValue; }
bool Value::isObject()const { return type_ == objectValue; }
ValueType Value::getType()const { return type_; }
void Value::swap(Value& other) {
	std::swap(data_, other.data_);
	std::swap(type_, other.type_);
}
bool Value::remove(const String& index) {
	if (isObject()) {
		return data_.o->erase(index);
	}
	return false;
}
void Value::append(const Value& value) {
	append(Value(value));
}
void Value::append(Value&& value) {
	assert(isArray() || isNull());
	if (isNull())
		new (this)Value(arrayValue);
	data_.a->push_back(move(value));
}
size_t Value::size()const {
	switch (type_) {
	case nullValue:
		return 0;
		break;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		return data_.s->size();
		break;
	case booleanValue:
		break;
	case arrayValue:
		return data_.a->size();
		break;
	case objectValue:
		return data_.o->size();
		break;
	default:
		break;
	}
	return 1;
}
bool Value::empty()const {
	switch (type_) {
	case nullValue:
		return true;
		break;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		return data_.s->empty();
		break;
	case booleanValue:
		break;
	case arrayValue:
		return data_.a->empty();
		break;
	case objectValue:
		return data_.o->empty();
		break;
	default:
		break;
	}
	return false;
}
bool Value::has(const String& key)const {
	if (!isObject())
		return false;
	return data_.o->find(key) != data_.o->end();
}
void Value::clear() {
	switch (type_) {
	case nullValue:
		break;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		data_.s->clear();
		break;
	case booleanValue:
		break;
	case arrayValue:
		data_.a->clear();
		break;
	case objectValue:
		data_.o->clear();
		break;
	default:
		break;
	}
	type_ = nullValue;
}
String Value::toFastString()const {
	Writer w;
	w.writeValue(*this);
	return w.getString();
}
String Value::toStyledString()const {
	Writer w;
	w.writeValue(*this, true);
	return w.getString();
}
bool Value::parse(const char* str) {
	Reader r(str);
	bool res = r.readValue(*this);
	if (!res)
		std::cerr << r.getErrorString() << std::endl;
	return res;
}
bool Value::parse(const String& str) {
	return parse(str.c_str());
}
#pragma endregion
#pragma region Reader
Reader::Reader(const char* str) :ptr_(str), start_(str),err_(nullptr) {}
String Reader::getErrorString() {
	String err;
	unsigned line = 1;
	for (const char* begin = start_; begin != ptr_; ++begin) {
		if (*begin == '\n')
			++line;
	}
	return err.append(err_).append(" in Line ").append(std::to_string(line));
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
bool Reader::readString(String& s) {
	nextChar();
	for (;;) {
		char ch = readNextCharBack();
		if (!ch)
			return err_ = "Missing '\"'", false;
		if (ch == '"')
			return true;
		if (ch == '\\')
			switch (readNextCharBack()) {
			case '"': s.push_back('"'); break;
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
	value = stringValue;
	return readString(*value.data_.s);
}
bool Reader::readArray(Value& value) {
	nextChar();
	JSON_CHECK(readWhitespace());
	value = arrayValue;
	if (readChar() == ']') {
		nextChar();
		return true;
	}
	for (;;) {
		Value tmp;
		JSON_CHECK(readValue(tmp));
		value.data_.a->emplace_back(move(tmp));
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
	value = objectValue;
	if (readChar() == '}') {
		nextChar();
		return true;
	}
	for (;;) {
		String key;
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
		JSON_CHECK(readValue((*value.data_.o)[key]));
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
		break;
	case 'n':
		return readNull();
	case 't':
		return readTrue(value);
	case 'f':
		return readFalse(value);
	case '[':
		return readArray(value);
	case '{':
		return readObject(value);
	case '"':
		return readString(value);
	default:
		return readNumber(value);
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
	//return true;
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
static bool hasEsecapingChar(const String& str) {
	for (auto c : str) {
		if (c == '\\' || c == '"' || c < 0x20 || c > 0x7F)
			return true;
	}
	return false;
}
static UInt utf8ToCodepoint(const char* s, const char* e) {
	const UInt REPLACEMENT_CHARACTER = 0xFFFD;

	UInt firstByte = static_cast<unsigned char>(*s);

	if (firstByte < 0x80)
		return firstByte;

	if (firstByte < 0xE0) {
		if (e - s < 2)
			return REPLACEMENT_CHARACTER;

		UInt calculated =
			((firstByte & 0x1F) << 6) | (static_cast<UInt>(s[1]) & 0x3F);
		s += 1;
		// oversized encoded characters are invalid
		return calculated < 0x80 ? REPLACEMENT_CHARACTER : calculated;
	}

	if (firstByte < 0xF0) {
		if (e - s < 3)
			return REPLACEMENT_CHARACTER;

		UInt calculated = ((firstByte & 0x0F) << 12) |
			((static_cast<UInt>(s[1]) & 0x3F) << 6) |
			(static_cast<UInt>(s[2]) & 0x3F);
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

		UInt calculated = ((firstByte & 0x07) << 18) |
			((static_cast<UInt>(s[1]) & 0x3F) << 12) |
			((static_cast<UInt>(s[2]) & 0x3F) << 6) |
			(static_cast<UInt>(s[3]) & 0x3F);
		s += 3;
		// oversized encoded characters are invalid
		return calculated < 0x10000 ? REPLACEMENT_CHARACTER : calculated;
	}

	return REPLACEMENT_CHARACTER;
}
static String toHex16Bit(UInt x) {
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
	const UInt hi = (x >> 8) & 0xff;
	const UInt lo = x & 0xff;
	String out_(4, ' ');
	out_[0] = hex2[2 * hi];
	out_[1] = hex2[2 * hi + 1];
	out_[2] = hex2[2 * lo];
	out_[3] = hex2[2 * lo + 1];
	return out_;
}
Writer::Writer():indent_(0){}
String Writer::getString()const { return out_; }
void Writer::writeNumber(double value) {
	String&& s = std::to_string(value);
	while (s.back() == '0') {
		if (*(&s.back() - 1) != '.')
			s.pop_back();
		else break;
	}
	out_.append(s);
}
void Writer::writeString(const String& value) {
	out_.push_back('"');
	if (hasEsecapingChar(value))
		for (auto& c : value) {
			unsigned codepoint;
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
				codepoint = utf8ToCodepoint(&c, &*value.end()); // modifies `c`
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
		for (auto& x : *value.data_.a) {
			writeValue(x);
			out_.push_back(',');
		}
		out_.pop_back();
	}
	out_.push_back(']');
}
void Writer::writeObject(const Value& value) {
	out_.push_back('{');
	if (!value.empty()) {
		for (auto& x : *value.data_.o) {
			writeString(x.first);
			out_.push_back(':');
			writeValue(x.second);
			out_.push_back(',');
		}
		out_.pop_back();
	}
	out_.push_back('}');
}
void Writer::writeStyledArray(const Value& value) {
	out_.push_back('[');
	if (!value.empty()) {
		writeNewline();
		++indent_;
		for (auto& x : *value.data_.a) {
			writeIndent();
			writeValue(x, true);
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
		for (auto& x : *value.data_.o) {
			writeIndent();
			writeString(x.first);
			out_.push_back(':');
			out_.push_back(' ');
			writeValue(x.second, true);
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
void Writer::writeValue(const Value& value, bool styled) {
	switch (value.type_) {
	case nullValue:
		out_.append("null", 4);
		break;
	case intValue:
		out_.append(std::to_string(value.data_.i64));
		break;
	case uintValue:
		out_.append(std::to_string(value.data_.u64));
		break;
	case realValue:
		writeNumber(value.data_.d);
		break;
	case stringValue:
		writeString(*value.data_.s);
		break;
	case booleanValue:
		value.asBool() ? out_.append("true", 4) : out_.append("false", 5);
		break;
	case arrayValue:
		styled ? writeStyledArray(value) : writeArray(value);
		break;
	case objectValue:
		styled ? writeStyledObject(value) : writeObject(value);
		break;
	default:
		break;
	}
}
void Writer::writeIndent() {
	out_.append(indent_, '\t');
}
void Writer::writeNewline() {
	out_.push_back('\n');
}
#pragma endregion
std::ostream& operator<<(std::ostream& os, const Value& value) {
	return os << value.toStyledString();
}
} // namespace Json

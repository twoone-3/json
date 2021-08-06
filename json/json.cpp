#include "json.h"
#include <charconv>

#define JSON_UTF8 0
#define JSON_INDENT 4

#define JSON_CHECK(expr) if (!(expr))return false;
#define JSON_SKIP for (char c = *cur_; (c == ' ' || c == '\t' || c == '\n' || c == '\r') && cur_ != end_;c = *++cur_);
#define JSON_CHECK_END if (cur_ == end_)return false;
#define JSON_ASSERT(expr) if (!(expr))exit(-1);

namespace json {
static void CodePointToUTF8(String& s, UInt u) {
	if (u <= 0x7F)
		s += static_cast<char>(u & 0xFF);
	else if (u <= 0x7FF) {
		s += static_cast<char>(0xC0 | (0xFF & (u >> 6)));
		s += static_cast<char>(0x80 | (0x3F & u));
	}
	else if (u <= 0xFFFF) {
		s += static_cast<char>(0xE0 | (0xFF & (u >> 12)));
		s += static_cast<char>(0x80 | (0x3F & (u >> 6)));
		s += static_cast<char>(0x80 | (0x3F & u));
	}
	else {
		s += static_cast<char>(0xF0 | (0xFF & (u >> 18)));
		s += static_cast<char>(0x80 | (0x3F & (u >> 12)));
		s += static_cast<char>(0x80 | (0x3F & (u >> 6)));
		s += static_cast<char>(0x80 | (0x3F & u));
	}
}
static bool hasEsecapingChar(const String& str) {
	for (auto& c : str) {
		if (c == '\\' || c == '"' || c < 0x20 || c > 0x7F)
			return true;
	}
	return false;
}
static UInt UTF8ToCodepoint(const char* s, const char* e) {
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
static void writeHex16Bit(String& s, UInt u) {
	constexpr const char hex2[] =
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
	const UInt hi = (u >> 8) & 0xFF;
	const UInt lo = u & 0xFF;
	s += hex2[2 * hi];
	s += hex2[2 * hi + 1];
	s += hex2[2 * lo];
	s += hex2[2 * lo + 1];
}

bool Value::Parser::parse(const String& str, Value& value) {
	return parse(str.c_str(), str.length(), value);
}
bool Value::Parser::parse(const char* str, size_t len, Value& value) {
	cur_ = str;
	begin_ = str;
	end_ = str + len;
	//Skip whitespace
	JSON_SKIP; JSON_CHECK_END;
	bool res = parseValue(value);
	JSON_SKIP;
	if (cur_ != end_)
		return error("Extra characters");
	return res;
}
String Value::Parser::getError() {
	String e;
	e += err_;
	e += " in line ";
	size_t line = 1;
	for (const char* i = begin_; i != cur_; ++i) {
		if (*i == '\n')
			++line;
	}
	char buffer[21]{ 0 };
	std::to_chars(buffer, buffer + sizeof buffer, line);
	e += buffer;
	return e;
}
bool Value::Parser::parseValue(Value& value) {
	switch (*cur_) {
	case 'n':return parseNull(value);
	case 't':return parseTrue(value);
	case 'f':return parseFalse(value);
	case '[':return parseArray(value);
	case '{':return parseObject(value);
	case '"':return parseString(value);
	default:return parseNumber(value);
	}
}
bool Value::Parser::parseNull(Value& value) {
	if (cur_[1] == 'u' && cur_[2] == 'l' && cur_[3] == 'l') {
		cur_ += 4;
		value = nullptr;
		return true;
	}
	return false;
}
bool Value::Parser::parseTrue(Value& value) {
	if (cur_[1] == 'r' && cur_[2] == 'u' && cur_[3] == 'e') {
		cur_ += 4;
		value = true;
		return true;
	}
	return false;
}
bool Value::Parser::parseFalse(Value& value) {
	if (cur_[1] == 'a' && cur_[2] == 'l' && cur_[3] == 's' && cur_[4] == 'e') {
		cur_ += 5;
		value = false;
		return true;
	}
	return false;
}
bool Value::Parser::parseString(Value& value) {
	value = STRING_T;
	return parseString(*value.data_.s);
}
bool Value::Parser::parseString(String& s) {
	char c;
	for (;;) {
		// "xxxx"
		// ^
		c = *++cur_;
		if (c == '\0')
			return error("missing '\"'");
		else if (c == '"') {
			++cur_;
			return true;
		}
		else if (c == '\\') {
			// "xx\nxx"
			//    ^
			switch (*++cur_) {
			case '"': s += '"'; break;
			case 'n': s += '\n'; break;
			case 'r': s += '\r'; break;
			case 't': s += '\t'; break;
			case 'f': s += '\f'; break;
			case 'b': s += '\b'; break;
			case '/': s += '/'; break;
			case '\\': s += '\\'; break;
			case 'u': {
				UInt u = 0;
				JSON_CHECK(parseHex4(u));
				if (u >= 0xD800 && u <= 0xDBFF) {
					// "x\u0123\u0234xx"
					//        ^
					if (cur_[1] == '\\' && cur_[2] == 'u') {
						cur_ += 2;
						UInt surrogatePair = 0;
						if (!parseHex4(surrogatePair))
							u = 0x10000 + ((u & 0x3FF) << 10) + (surrogatePair & 0x3FF);
						else
							return error("invalid character");
					}
					else
						return error("missing surrogate pair");
				}
				CodePointToUTF8(s, u);
			}break;
			default:
				return error("invalid escape");
			}
		}
		else
			s += c;
	}
	return false;
}
bool Value::Parser::parseHex4(UInt& u) {
	//u = 0;
	char ch = 0;
	for (UInt i = 0; i < 4; ++i) {
		u <<= 4;
		// "x\u0123x"
		//    ^
		ch = *++cur_;
		if (ch >= '0' && ch <= '9')
			u |= ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			u |= ch - 'a' + 10;
		else if (ch >= 'A' && ch <= 'F')
			u |= ch - 'A' + 10;
		else
			return error("invalid character");
	}
	return true;
}
bool Value::Parser::parseArray(Value& value) {
	++cur_;
	JSON_SKIP;
	value = ARRAY_T;
	//empty array
	if (*cur_ == ']')
		return ++cur_, true;
	for (;;) {
		value.data_.a->push_back(nullptr);
		JSON_SKIP;
		JSON_CHECK(parseValue(value.data_.a->back()));
		JSON_SKIP;
		char ch = *cur_;
		if (ch == ',')
			++cur_;
		else if (ch == ']')
			return ++cur_, true;
		else
			return error("missing ',' or ']'");
	}
	return false;
}
bool Value::Parser::parseObject(Value& value) {
	++cur_;
	JSON_SKIP;
	value = OBJECT_T;
	//empty object
	if (*cur_ == '}')
		return ++cur_, true;
	for (;;) {
		String key;
		JSON_SKIP;
		if (*cur_ != '"')
			return error("missing '\"'");
		JSON_CHECK(parseString(key));
		JSON_SKIP;
		if (*cur_ != ':')
			return error("missing ':'");
		++cur_;
		JSON_SKIP;
		JSON_CHECK(parseValue(value.data_.o->operator[](key)));
		JSON_SKIP;
		char ch = *cur_;
		if (ch == ',')
			++cur_;
		else if (ch == '}')
			return ++cur_, true;
		else
			return error("missing ',' or '}'");
	}
	return false;
}
bool Value::Parser::parseNumber(Value& value) {
	const char* p = cur_;
	// stopgap for already consumed character
	char c = *p;
	// skip '-'
	if (c == '-')
		c = *++p;
	// integral part
	while (c >= '0' && c <= '9')
		c = *++p;
	// fractional part
	if (c == '.') {
		c = *++p;
		while (c >= '0' && c <= '9')
			c = *++p;
	}
	// exponential part
	if (c == 'e' || c == 'E') {
		c = *++p;
		if (c == '+' || c == '-')
			c = *++p;
		while (c >= '0' && c <= '9')
			c = *++p;
	}
	// check out of range
	if (c == '\0')
		return error("unexpected_ending_character");
	double num;
	std::from_chars(cur_, p, num);
	cur_ = p;
	value = num;
	return true;
}
bool Value::Parser::error(const char* err) {
	err_ = err;
	return false;
}

void Value::Writer::write(const Value& value, bool styled) {
	switch (value.type_) {
	case NULL_T:return writeNull();
	case INT_T:return writeInt(value);
	case UINT_T:return writeUInt(value);
	case REAL_T:return writeDouble(value);
	case STRING_T:return writeString(value);
	case BOOL_T:return writeBool(value);
	case ARRAY_T:return styled ? writePrettyArray(value) : writeArray(value);
	case OBJECT_T:return styled ? writePrettyObject(value) : writeObject(value);
	}
}
String Value::Writer::getOutput() {
	return out_;
}
void Value::Writer::writeIndent() {
	out_.append(indent_ * JSON_INDENT, ' ');
}
void Value::Writer::writeNull() {
	out_.append("null", 4);
}
void Value::Writer::writeBool(const Value& value) {
	value.data_.b ? out_.append("true", 4) : out_.append("false", 5);
}
void Value::Writer::writeInt(const Value& value) {
	char buffer[21]{ 0 };
	std::to_chars(buffer, buffer + sizeof buffer, value.data_.i64);
	out_.append(buffer);
}
void Value::Writer::writeUInt(const Value& value) {
	char buffer[21]{ 0 };
	std::to_chars(buffer, buffer + sizeof buffer, value.data_.u64);
	out_.append(buffer);
}
void Value::Writer::writeDouble(const Value& value) {
	char buffer[21]{ 0 };
	std::to_chars(buffer, buffer + sizeof buffer, value.data_.d);
	out_.append(buffer);
}
void Value::Writer::writeString(const Value& value) {
	const String& str = *value.data_.s;
	out_ += '"';
	if (hasEsecapingChar(str))
		for (auto& c : str) {
			UInt codepoint;
			switch (c) {
			case '\"':
				out_ += '\\';
				out_ += '"';
				break;
			case '\\':
				out_ += '\\';
				out_ += '\\';
				break;
			case '\b':
				out_ += '\\';
				out_ += 'b';
				break;
			case '\f':
				out_ += '\\';
				out_ += 'f';
				break;
			case '\n':
				out_ += '\\';
				out_ += 'n';
				break;
			case '\r':
				out_ += '\\';
				out_ += 'r';
				break;
			case '\t':
				out_ += '\\';
				out_ += 't';
				break;
			default:
#if JSON_UTF8
				if (uint8_t(c) < 0x20) {
					out_ += "\\u";
					writeHex16Bit(out_, c);
				}
				else
					out_ += c;
#else
				codepoint = UTF8ToCodepoint(&c, &str.back()); // modifies `c`
				if (codepoint < 0x20) {
					out_ += "\\u";
					writeHex16Bit(out_, codepoint);
				}
				else if (codepoint < 0x80) {
					out_ += static_cast<char>(codepoint);
				}
				else if (codepoint < 0x10000) {
					// Basic Multilingual Plane
					out_ += "\\u";
					writeHex16Bit(out_, codepoint);
				}
				else {
					puts("ssssssssss___");
					// Extended Unicode. Encode 20 bits as a surrogate pair.
					codepoint -= 0x10000;
					out_ += "\\u";
					writeHex16Bit(out_, 0xD800 + ((codepoint >> 10) & 0x3FF));
					writeHex16Bit(out_, 0xDC00 + (codepoint & 0x3FF));
				}
#endif
				break;
			}
		}
	else
		out_.append(str);
	out_ += '"';
}
void Value::Writer::writeArray(const Value& value) {
	out_ += '[';
	if (!value.empty()) {
		for (auto& x : *value.data_.a) {
			write(x, false);
			out_ += ',';
		}
		out_.pop_back();
	}
	out_ += ']';

}
void Value::Writer::writeObject(const Value& value) {
	out_ += '{';
	if (!value.empty()) {
		for (auto& x : *value.data_.o) {
			writeString(x.first);
			out_ += ':';
			write(x.second, false);
			out_ += ',';
		}
		out_.pop_back();
	}
	out_ += '}';
}
void Value::Writer::writePrettyArray(const Value& value) {
	out_ += '[';
	if (!value.empty()) {
		out_ += '\n';
		++indent_;
		for (auto& x : *value.data_.a) {
			writeIndent();
			write(x, true);
			out_ += ',';
			out_ += '\n';
		}
		--indent_;
		out_.pop_back();
		out_.pop_back();
		out_ += '\n';
		writeIndent();
	}
	out_ += ']';
}
void Value::Writer::writePrettyObject(const Value& value) {
	out_ += '{';
	if (!value.empty()) {
		out_ += '\n';
		++indent_;
		for (auto& x : *value.data_.o) {
			writeIndent();
			writeString(x.first);
			out_ += ':';
			out_ += ' ';
			write(x.second, true);
			out_ += ',';
			out_ += '\n';
		}
		--indent_;
		out_.pop_back();
		out_.pop_back();
		out_ += '\n';
		writeIndent();
	}
	out_ += '}';
}

Value::Value() : type_(NULL_T) { data_.u64 = 0; }
Value::Value(nullptr_t) : type_(NULL_T) { data_.u64 = 0; }
Value::Value(bool b) : type_(BOOL_T) { data_.u64 = b; }
Value::Value(Int num) : type_(INT_T) { data_.i64 = num; }
Value::Value(UInt num) : type_(UINT_T) { data_.u64 = num; }
Value::Value(Int64 num) : type_(INT_T) { data_.i64 = num; }
Value::Value(UInt64 num) : type_(UINT_T) { data_.u64 = num; }
Value::Value(Float num) : type_(REAL_T) { data_.d = num; }
Value::Value(Double num) : type_(REAL_T) { data_.d = num; }
Value::Value(const char* s) : type_(STRING_T) { data_.s = new String(s); }
Value::Value(const String& s) : type_(STRING_T) { data_.s = new String(s); }
Value::Value(ValueType type) : type_(type) {
	data_.u64 = 0;
	switch (type_) {
	case NULL_T:
		break;
	case INT_T:
		break;
	case UINT_T:
		break;
	case REAL_T:
		break;
	case STRING_T:
		data_.s = new String;
		break;
	case BOOL_T:
		break;
	case ARRAY_T:
		data_.a = new Array;
		break;
	case OBJECT_T:
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
	case NULL_T:
		break;
	case INT_T:
		data_.i64 = other.data_.i64;
		break;
	case UINT_T:
		data_.u64 = other.data_.u64;
		break;
	case REAL_T:
		data_.d = other.data_.d;
		break;
	case STRING_T:
		data_.s = new String(*other.data_.s);
		break;
	case BOOL_T:
		data_.b = other.data_.b;
		break;
	case ARRAY_T:
		data_.a = new Array(*other.data_.a);
		break;
	case OBJECT_T:
		data_.o = new Object(*other.data_.o);
		break;
	default:
		break;
	}
};
Value::Value(Value&& other)noexcept {
	data_ = other.data_;
	type_ = other.type_;
	other.type_ = NULL_T;
	other.data_.u64 = 0;
};
Value::~Value() {
	switch (type_) {
	case NULL_T:
		break;
	case INT_T:
		break;
	case UINT_T:
		break;
	case REAL_T:
		break;
	case STRING_T:
		delete data_.s;
		break;
	case BOOL_T:
		break;
	case ARRAY_T:
		delete data_.a;
		break;
	case OBJECT_T:
		delete data_.o;
		break;
	default:
		break;
	}
	type_ = NULL_T;
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
	case NULL_T:
		return true;
	case INT_T:
		return data_.i64 == other.data_.i64;
	case UINT_T:
		return data_.u64 == other.data_.u64;
	case REAL_T:
		return data_.d == other.data_.d;
	case STRING_T:
		return *data_.s == *other.data_.s;
	case BOOL_T:
		return data_.b == other.data_.b;
	case ARRAY_T:
		return *data_.a == *other.data_.a;
	case OBJECT_T:
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
	JSON_ASSERT(isObject() || isNull());
	if (isNull())
		new (this)Value(OBJECT_T);
	return data_.o->operator[](index);
}
Value& Value::operator[](size_t index) {
	JSON_ASSERT(isArray() || isNull());
	if (isNull())
		new (this)Value(ARRAY_T);
	return data_.a->operator[](index);
}
void Value::insert(const String& index, Value&& value) {
	JSON_ASSERT(isObject() || isNull());
	if (isNull())
		new (this)Value(OBJECT_T);
	data_.o->emplace(index, value);
}
bool Value::asBool()const { JSON_ASSERT(isBool()); return data_.b; }
Int Value::asInt()const { JSON_ASSERT(isNumber()); return data_.i; }
UInt Value::asUInt()const { JSON_ASSERT(isNumber()); return data_.u; }
Int64 Value::asInt64()const { JSON_ASSERT(isNumber()); return data_.i64; }
UInt64 Value::asUInt64()const { JSON_ASSERT(isNumber()); return data_.u64; }
Float Value::asFloat()const { JSON_ASSERT(isNumber()); return data_.f; }
Double Value::asDouble()const { JSON_ASSERT(isNumber()); return data_.d; }
String Value::asString()const { JSON_ASSERT(isString());	return *data_.s; }
Array Value::asArray()const { JSON_ASSERT(isArray()); return *data_.a; }
Object Value::asObject()const { JSON_ASSERT(isObject()); return *data_.o; }
bool Value::isNull()const { return type_ == NULL_T; }
bool Value::isBool()const { return type_ == BOOL_T; }
bool Value::isNumber()const { return type_ == INT_T || type_ == UINT_T || type_ == REAL_T; }
bool Value::isString()const { return type_ == STRING_T; }
bool Value::isArray()const { return type_ == ARRAY_T; }
bool Value::isObject()const { return type_ == OBJECT_T; }
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
	JSON_ASSERT(isArray() || isNull());
	if (isNull())
		new (this)Value(ARRAY_T);
	data_.a->push_back(move(value));
}
size_t Value::size()const {
	switch (type_) {
	case NULL_T:
		return 0;
	case INT_T:
		break;
	case UINT_T:
		break;
	case REAL_T:
		break;
	case STRING_T:
		return data_.s->size();
	case BOOL_T:
		break;
	case ARRAY_T:
		return data_.a->size();
	case OBJECT_T:
		return data_.o->size();
	default:
		break;
	}
	return 1;
}
bool Value::empty()const {
	switch (type_) {
	case NULL_T:
		return true;
	case INT_T:
		break;
	case UINT_T:
		break;
	case REAL_T:
		break;
	case STRING_T:
		return data_.s->empty();
	case BOOL_T:
		break;
	case ARRAY_T:
		return data_.a->empty();
	case OBJECT_T:
		return data_.o->empty();
	default:
		break;
	}
	return false;
}
bool Value::has(const String& key)const {
	if (isObject())
		return data_.o->find(key) != data_.o->end();
	return false;
}
void Value::clear() {
	switch (type_) {
	case NULL_T:
		break;
	case INT_T:
		break;
	case UINT_T:
		break;
	case REAL_T:
		break;
	case STRING_T:
		data_.s->clear();
		break;
	case BOOL_T:
		break;
	case ARRAY_T:
		data_.a->clear();
		break;
	case OBJECT_T:
		data_.o->clear();
		break;
	default:
		break;
	}
	type_ = NULL_T;
}
String Value::toShortString()const {
	Writer w;
	w.write(*this, false);
	return w.getOutput();
}
String Value::toStyledString()const {
	Writer w;
	w.write(*this, true);
	return w.getOutput();
}
Value::operator String()const {
	return toShortString();
}
std::ostream& operator<<(std::ostream& os, const Value& value) {
	return os << value.toStyledString();
}
Value Parse(const String& s) {
	Value value;
	Value::Parser parser;
	if (!parser.parse(s, value))
		std::cerr << parser.getError() << std::endl;
	return value;
}
} // namespace Json

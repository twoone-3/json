#include "json.h"

#define JSON_CHECK(expression) do { ErrorCode tmp = expression; if (tmp != OK)return tmp; } while (0)
#define JSON_SKIP JSON_CHECK(skipWhiteSpace(cur))
#define JSON_UTF8 0

namespace json {
static constexpr const char* ErrorToString(ErrorCode code) {
	switch (code) {
	case json::OK:
		return "OK";
	case json::INVALID_CHARACTER:
		return "Invalid character";
	case json::INVALID_ESCAPE:
		return "Invalid escape";
	case json::MISSING_QUOTE:
		return "Missing '\"'";
	case json::MISSING_NULL:
		return "Missing 'null'";
	case json::MISSING_TRUE:
		return "Missing 'true'";
	case json::MISSING_FALSE:
		return "Missing 'false'";
	case json::MISSING_COMMAS_OR_BRACKETS:
		return "Missing ',' or ']'";
	case json::MISSING_COMMAS_OR_BRACES:
		return "Missing ',' or '}'";
	case json::MISSING_COLON:
		return "Missing ':'";
	case json::MISSING_SURROGATE_PAIR:
		return "Missing another '\\u' token to begin the second half of a unicode surrogate pair";
	case json::UNEXPECTED_ENDING_CHARACTER:
		return "Unexpected '\\0'";
	default:
		return "Unknow";
	}
}
static void CodePointToUTF8(String& s, UInt u) {
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
static bool hasEsecapingChar(const String& str) {
	for (auto c : str) {
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
static String toHex16Bit(UInt x) {
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
	const UInt hi = (x >> 8) & 0xff;
	const UInt lo = x & 0xff;
	String out(4, ' ');
	out[0] = hex2[2 * hi];
	out[1] = hex2[2 * hi + 1];
	out[2] = hex2[2 * lo];
	out[3] = hex2[2 * lo + 1];
	return out;
}
static ErrorCode skipWhiteSpace(const char*& cur) {
	for (;;) {
		switch (*cur) {
		case'\t':break;
		case'\n':break;
		case'\r':break;
		case' ':break;
#if 0
		case'/':
			++cur;
			//单行注释
			if (*cur == '/') {
				++cur;
				while (*cur != '\n') {
					++cur;
				}
			}
			//多行注释
			else if (*cur == '*') {
				++cur;
				while (*cur != '*' || cur[1] != '/') {
					if (*cur == '\0') {
						return error_ = "Missing '*/'", false;
					}
					++cur;
				}
				++cur;
			}
			else
				return error_ = "Invalid comment", false;
			break;
#endif
		default:
			return OK;
		}
		++cur;
	}
}
static ErrorCode parseHex4(const char*& cur, UInt& u) {
	//u = 0;
	char ch = 0;
	for (UInt i = 0; i < 4; ++i) {
		u <<= 4;
		// cur:
		// "x\u0123x"
		//    ^
		ch = *++cur;
		if (ch >= '0' && ch <= '9')
			u |= ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			u |= ch - 'a' + 10;
		else if (ch >= 'A' && ch <= 'F')
			u |= ch - 'A' + 10;
		else
			return INVALID_CHARACTER;
	}
	return OK;
}
static ErrorCode parseString(const char*& cur, String& s) {
	for (;;) {
		// cur:
		// "xxxx"
		// ^
		char ch = *++cur;
		if (!ch)
			return MISSING_QUOTE;
		else if (ch == '"') {
			++cur;
			return OK;
		}
		else if (ch == '\\') {
			// cur:
			// "xx\nxx"
			//    ^
			switch (*++cur) {
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
				JSON_CHECK(parseHex4(cur, u));
				if (u >= 0xD800 && u <= 0xDBFF) {
					// cur:
					// "x\u0123\u0234xx"
					//        ^
					if (cur[1] == '\\' && cur[2] == 'u') {
						cur += 2;
						UInt surrogatePair = 0;
						if (!parseHex4(cur, surrogatePair))
							u = 0x10000 + ((u & 0x3FF) << 10) + (surrogatePair & 0x3FF);
						else
							return INVALID_CHARACTER;
					}
					else
						return MISSING_SURROGATE_PAIR;
				}
				CodePointToUTF8(s, u);
			}break;
			default:
				return INVALID_ESCAPE;
			}
		}
		else
			s += ch;
	}
	return OK;
}
static void writeString(String& out, const String& str) {
	out += '"';
	if (hasEsecapingChar(str))
		for (auto c : str) {
			UInt codepoint;
			switch (c) {
			case '\"':
				out += '\\';
				out += '"';
				break;
			case '\\':
				out += '\\';
				out += '\\';
				break;
			case '\b':
				out += '\\';
				out += 'b';
				break;
			case '\f':
				out += '\\';
				out += 'f';
				break;
			case '\n':
				out += '\\';
				out += 'n';
				break;
			case '\r':
				out += '\\';
				out += 'r';
				break;
			case '\t':
				out += '\\';
				out += 't';
				break;
			default:
#if JSON_UTF8
				if (uint8_t(c) < 0x20)
					out.append("\\u").append(toHex16Bit(c));
				else
					out += c);
#else
				codepoint = UTF8ToCodepoint(&c, &*str.end()); // modifies `c`
				if (codepoint < 0x20) {
					out.append("\\u").append(toHex16Bit(codepoint));
				}
				else if (codepoint < 0x80) {
					out += static_cast<char>(codepoint);
				}
				else if (codepoint < 0x10000) {
					// Basic Multilingual Plane
					out.append("\\u").append(toHex16Bit(codepoint));
				}
				else {
					// Extended Unicode. Encode 20 bits as a surrogate pair.
					codepoint -= 0x10000;
					out.append("\\u").append(toHex16Bit(0xd800 + ((codepoint >> 10) & 0x3ff)));
					out.append("\\u").append(toHex16Bit(0xdc00 + (codepoint & 0x3ff)));
				}
#endif
				break;
			}
		}
	else
		out.append(str);
	out += '"';
}
Value::Value() :type_(nullValue) { data_.u64 = 0; }
Value::Value(nullptr_t) : type_(nullValue) { data_.u64 = 0; }
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
		return OK;
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
bool Value::asBool()const { assert(isBool()); return data_.b; }
Int Value::asInt()const { assert(isNumber()); return data_.i; }
UInt Value::asUInt()const { assert(isNumber()); return data_.u; }
Int64 Value::asInt64()const { assert(isNumber()); return data_.i64; }
UInt64 Value::asUInt64()const { assert(isNumber()); return data_.u64; }
Float Value::asFloat()const { assert(isNumber()); return data_.f; }
Double Value::asDouble()const { assert(isNumber()); return data_.d; }
String Value::asString()const { assert(isString());	return *data_.s; }
Array Value::asArray()const { assert(isArray()); return *data_.a; }
Object Value::asObject()const { assert(isObject()); return *data_.o; }
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
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		return data_.s->size();
	case booleanValue:
		break;
	case arrayValue:
		return data_.a->size();
	case objectValue:
		return data_.o->size();
	default:
		break;
	}
	return 1;
}
bool Value::empty()const {
	switch (type_) {
	case nullValue:
		return OK;
	case intValue:
		break;
	case uintValue:
		break;
	case realValue:
		break;
	case stringValue:
		return data_.s->empty();
	case booleanValue:
		break;
	case arrayValue:
		return data_.a->empty();
	case objectValue:
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
String Value::toShortString()const {
	String out;
	UInt indent = 0;
	_toString(out, indent, false);
	return out;
}
String Value::toStyledString()const {
	String out;
	UInt indent = 0;
	_toString(out, indent, true);
	return out;
}
ErrorCode Value::parse(const char* str) {
	const char* cur = str;
	JSON_SKIP;
	ErrorCode err = _parseValue(cur);
	if (err != OK) {
		size_t line = 1;
		for (const char* i = str; i != cur; ++i) {
			if (*i == '\n')++line;
		}
		std::cerr << err << " in line " << line << std::endl;
	}
	return err;
}
ErrorCode Value::parse(const String& str) {
	return parse(str.c_str());
}
ErrorCode Value::_parseValue(const char*& cur) {
	switch (*cur) {
	case '\0':
		abort();
		break;
	case 'n': {
		if (cur[1] == 'u' && cur[2] == 'l' && cur[3] == 'l')
			return cur += 4, OK;
		else
			return MISSING_NULL;
	}
	case 't': {
		if (cur[1] == 'r' && cur[2] == 'u' && cur[3] == 'e')
			return *this = true, cur += 4, OK;
		else
			return MISSING_TRUE;
	}
	case 'f': {
		if (cur[1] == 'a' && cur[2] == 'l' && cur[3] == 's' && cur[4] == 'e')
			return *this = false, cur += 5, OK;
		else
			return MISSING_FALSE;
	}
	case '[': {
		++cur;
		JSON_SKIP;
		*this = arrayValue;
		if (*cur == ']') {
			++cur;
			return OK;
		}
		for (;;) {
			data_.a->push_back({});
			JSON_SKIP;
			JSON_CHECK(data_.a->back()._parseValue(cur));
			JSON_SKIP;
			if (*cur == ',')
				++cur;
			else if (*cur == ']') {
				++cur; return OK;
			}
			else
				return MISSING_COMMAS_OR_BRACKETS;
		}
	}
	case '{': {
		++cur;
		JSON_SKIP;
		*this = objectValue;
		if (*cur == '}') {
			++cur;
			return OK;
		}
		for (;;) {
			String key;
			JSON_SKIP;
			if (*cur != '"') {
				return MISSING_QUOTE;
			}
			JSON_CHECK(parseString(cur, key));
			JSON_SKIP;
			if (*cur != ':') {
				return MISSING_COLON;
			}
			++cur;
			JSON_SKIP;
			JSON_CHECK(data_.o->operator[](key)._parseValue(cur));
			JSON_SKIP;
			if (*cur == ',')
				++cur;
			else if (*cur == '}') {
				++cur; return OK;
			}
			else
				return MISSING_COMMAS_OR_BRACES;
		}
	}
	case '"': {
		*this = stringValue;
		JSON_CHECK(parseString(cur, *data_.s));
		return OK;
	}
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '-': {
		const char* p = cur;
		char c = '0'; // stopgap for already consumed character
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
		if (!c)
			return UNEXPECTED_ENDING_CHARACTER;
		double num = 0.0;
		std::from_chars(cur, p, num);
		cur = p;
		*this = num;
		return OK;
	}
	default:
		return INVALID_CHARACTER;
	}
}
void Value::_toString(String& out, UInt& indent, bool styled)const {
	switch (type_) {
	case nullValue:
		out.append("null", 4);
		break;
	case intValue: {
		char buffer[21]{ 0 };
		std::to_chars(buffer, buffer + sizeof buffer, data_.i64);
		out.append(buffer);
		break;
	}
	case uintValue: {
		char buffer[21]{ 0 };
		std::to_chars(buffer, buffer + sizeof buffer, data_.u64);
		out.append(buffer);
		break;
	}
	case realValue: {
		char buffer[21]{ 0 };
		std::to_chars(buffer, buffer + sizeof buffer, data_.d);
		out.append(buffer);
		break;
	}
	case stringValue:
		writeString(out, *data_.s);
		break;
	case booleanValue:
		asBool() ? out.append("true", 4) : out.append("false", 5);
		break;
	case arrayValue:
		if (styled) {
			out += '[';
			if (!empty()) {
				out += '\n';
				++indent;
				for (auto& x : *data_.a) {
					out.append(indent, '\t');
					x._toString(out, indent, styled);
					out += ',';
					out += '\n';
				}
				--indent;
				out.pop_back();
				out.pop_back();
				out += '\n';
				out.append(indent, '\t');
			}
			out += ']';
		}
		else {
			out += '[';
			if (!empty()) {
				for (auto& x : *data_.a) {
					x._toString(out, indent, styled);
					out += ',';
				}
				out.pop_back();
			}
			out += ']';
		}
		break;
	case objectValue:
		if (styled) {
			out += '{';
			if (!empty()) {
				out += '\n';
				++indent;
				for (auto& x : *data_.o) {
					out.append(indent, '\t');
					writeString(out, x.first);
					out += ':';
					out += ' ';
					x.second._toString(out, indent, styled);
					out += ',';
					out += '\n';
				}
				--indent;
				out.pop_back();
				out.pop_back();
				out += '\n';
				out.append(indent, '\t');
			}
			out += '}';
		}
		else {
			out += '{';
			if (!empty()) {
				for (auto& x : *data_.o) {
					writeString(out, x.first);
					out += ':';
					x.second._toString(out, indent, styled);
					out += ',';
				}
				out.pop_back();
			}
			out += '}';
		}
		break;
	default:
		break;
	}

}
std::ostream& operator<<(std::ostream& os, const Value& value) {
	return os << value.toStyledString();
}
std::ostream& operator<<(std::ostream& os, ErrorCode err) {
	return os << ErrorToString(err);
}
} // namespace Json

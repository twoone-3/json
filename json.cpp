#include "json.h"
#include <charconv>

#define JSON_SKIP_WHITE_SPACE if(!skipWhiteSpace())return false
#define JSON_CHECK_OUT_OF_RANGE if (cur_ == end_)return error("unexpected ending character")
#define JSON_ASSERT(expr,msg) if (!(expr))Fatal(msg)

namespace json {
static void Fatal(const char* msg) {
	std::cerr << msg << std::endl;
	exit(-1);
}
static void CodePointToUTF8(std::string& s, unsigned u) {
	if (u <= 0x7F) {
		s += static_cast<char>(u & 0xFF);
	}
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
static unsigned UTF8ToCodepoint(const char*& cur, const char* end) {
	constexpr unsigned REPLACEMENT_CHARACTER = 0xFFFD;

	unsigned firstByte = static_cast<unsigned char>(*cur);

	if (firstByte < 0x80)
		return firstByte;

	if (firstByte < 0xE0) {
		if (end - cur < 2)
			return REPLACEMENT_CHARACTER;

		unsigned calculated =
			((firstByte & 0x1F) << 6) | (static_cast<unsigned>(cur[1]) & 0x3F);
		cur += 1;
		// oversized encoded characters are invalid
		return calculated < 0x80 ? REPLACEMENT_CHARACTER : calculated;
	}

	if (firstByte < 0xF0) {
		if (end - cur < 3)
			return REPLACEMENT_CHARACTER;

		unsigned calculated = ((firstByte & 0x0F) << 12) |
			((static_cast<unsigned>(cur[1]) & 0x3F) << 6) |
			(static_cast<unsigned>(cur[2]) & 0x3F);
		cur += 2;
		// surrogates aren't valid codepoints itself
		// shouldn't be UTF-8 encoded
		if (calculated >= 0xD800 && calculated <= 0xDFFF)
			return REPLACEMENT_CHARACTER;
		// oversized encoded characters are invalid
		return calculated < 0x800 ? REPLACEMENT_CHARACTER : calculated;
	}

	if (firstByte < 0xF8) {
		if (end - cur < 4)
			return REPLACEMENT_CHARACTER;

		unsigned calculated = ((firstByte & 0x07) << 18) |
			((static_cast<unsigned>(cur[1]) & 0x3F) << 12) |
			((static_cast<unsigned>(cur[2]) & 0x3F) << 6) |
			(static_cast<unsigned>(cur[3]) & 0x3F);
		cur += 3;
		// oversized encoded characters are invalid
		return calculated < 0x10000 ? REPLACEMENT_CHARACTER : calculated;
	}

	return REPLACEMENT_CHARACTER;
}

Reader::Reader() :
	cur_(nullptr), begin_(nullptr), end_(nullptr), err_(nullptr),
	allow_comments_(false) {
}
Reader& Reader::allowComments() {
	allow_comments_ = true;
	return *this;
}
bool Reader::parse(std::string_view str, Value& value) {
	cur_ = str.data();
	begin_ = str.data();
	end_ = str.data() + str.length();
	// empty string
	JSON_CHECK_OUT_OF_RANGE;
	// skip BOM
	if (cur_[0] == 0xEF && cur_[1] == 0xBB && cur_[2] == 0xBF)
		cur_ += 3;
	// skip whitespace
	JSON_SKIP_WHITE_SPACE;
	bool result = parseValue(value);
	if (!result)
		value = nullptr;
	return result;
}
bool Reader::parseFile(std::string_view filename, Value& value) {
	FILE* f = nullptr;
	fopen_s(&f, filename.data(), "rb");
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	char* text = new char[size + 1];
	rewind(f);
	fread(text, sizeof(char), size, f);
	text[size] = '\0';
	bool result = parse(std::string_view(text, size), value);
	delete[] text;
	return result;
}
std::string Reader::getError()const {
	std::string err(err_);
	err += " in line ";
	unsigned line = 1;
	// count the number of rows from begin_ to cur_
	for (const char* cur = begin_; cur != cur_; ++cur) {
		if (*cur == '\n')
			++line;
	}
	char buffer[21]{ 0 };
	std::to_chars(buffer, buffer + sizeof buffer, line);
	err += buffer;
	return err;
}
bool Reader::parseValue(Value& value) {
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
bool Reader::parseNull(Value& value) {
	if (cur_[1] == 'u' && cur_[2] == 'l' && cur_[3] == 'l') {
		cur_ += 4;
		value = nullptr;
		return true;
	}
	return error("Missing 'null'");
}
bool Reader::parseTrue(Value& value) {
	if (cur_[1] == 'r' && cur_[2] == 'u' && cur_[3] == 'e') {
		cur_ += 4;
		value = true;
		return true;
	}
	return error("Missing 'true'");
}
bool Reader::parseFalse(Value& value) {
	if (cur_[1] == 'a' && cur_[2] == 'l' && cur_[3] == 's' && cur_[4] == 'e') {
		cur_ += 5;
		value = false;
		return true;
	}
	return error("Missing 'false'");
}
bool Reader::parseString(Value& value) {
	value = std::string();
	return parseString(value.asString());
}
bool Reader::parseString(std::string& s) {
	while (true) {
		// "xxxx"
		// ^
		char c = *++cur_;
		switch (c) {
		case '\0':
			return error("missing '\"'");
		case '"':
			++cur_;
			return true;
		case '\\':
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
				unsigned u = 0;
				if (!parseHex4(u))
					return false;
				if (u >= 0xD800 && u <= 0xDBFF) {
					// "x\u0123\u0234xx"
					//        ^
					if (cur_[1] == '\\' && cur_[2] == 'u') {
						cur_ += 2;
						unsigned surrogatePair = 0;
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
			break;
		default:
			if (static_cast<unsigned char>(c) < ' ')
				return error("The ASCII code of the characters in the string must be greater than 32");
			s += c;
			break;
		}
	}
}
bool Reader::parseHex4(unsigned& u) {
	//u = 0;
	char ch = 0;
	for (unsigned i = 0; i < 4; ++i) {
		u <<= 4;
		// "x\u0123x"
		//    ^
		ch = *++cur_;
		JSON_CHECK_OUT_OF_RANGE;
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
bool Reader::parseArray(Value& value) {
	++cur_;
	value.data_ = Array();
	JSON_SKIP_WHITE_SPACE;
	//empty array
	if (*cur_ == ']')
		return ++cur_, true;
	while (true) {
		value.asArray().push_back(nullptr);
		JSON_SKIP_WHITE_SPACE;
		if (!parseValue(value.asArray().back()))
			return false;
		JSON_SKIP_WHITE_SPACE;
		char ch = *cur_;
		if (ch == ',')
			++cur_;
		else if (ch == ']')
			return ++cur_, true;
		else
			return error("missing ',' or ']'");
	}
}
bool Reader::parseObject(Value& value) {
	++cur_;
	value.data_ = Object();
	JSON_SKIP_WHITE_SPACE;
	//empty object
	if (*cur_ == '}')
		return ++cur_, true;
	while (true) {
		std::string key;
		JSON_SKIP_WHITE_SPACE;
		if (*cur_ != '"')
			return error("missing '\"'");
		if (!parseString(key))
			return false;
		JSON_SKIP_WHITE_SPACE;
		if (*cur_ != ':')
			return error("missing ':'");
		++cur_;
		JSON_CHECK_OUT_OF_RANGE;
		JSON_SKIP_WHITE_SPACE;
		if (!parseValue(value.asObject().operator[](key)))
			return false;
		JSON_SKIP_WHITE_SPACE;
		char ch = *cur_;
		if (ch == ',')
			++cur_;
		else if (ch == '}')
			return ++cur_, true;
		else
			return error("missing ',' or '}'");
	}
}
bool Reader::parseNumber(Value& value) {
	/* Refer to https://www.json.org/img/number.png */
	const uint8_t c = *cur_;
	if (c != '-' && (c < '0' || c > '9'))
		return error("invalid character");
	double num = 0.0;
	auto result = std::from_chars(cur_, end_, num);
	cur_ = result.ptr;
	if (result.ec == std::errc::result_out_of_range)
		return error("number out of range");
	value = num;
	return true;
}
bool Reader::skipWhiteSpace() {
	while (true) {
		switch (*cur_) {
		case '\0':
			return error("unexpected ending character");
		case '\t':
		case '\n':
		case '\r':
		case ' ':
			break;
		case '/':
			// If it is true, it is allowed to parse comments in the style of'//' or'/**/'
			if (!allow_comments_)
				return error("comments are not allowed here");
			++cur_;
			if (*cur_ == '/') {
				while (*++cur_ != '\n') {
					JSON_CHECK_OUT_OF_RANGE;
				}
				++cur_;
				break;
			}
			else if (*cur_ == '*') {
				while (*++cur_ != '*') {
					JSON_CHECK_OUT_OF_RANGE;
				}
				if (*++cur_ == '/') {
					++cur_;
					break;
				}
			}
			else {
				return error("invalied comment style");
			}
		default:
			return true;
		}
		++cur_;
		JSON_CHECK_OUT_OF_RANGE;
	}
	return true;
}
bool Reader::error(const char* err) {
	err_ = err;
	return false;
}

Writer::Writer() :
	out_(), depth_of_indentation_(0),
	emit_utf8_(false) {
}
Writer& Writer::emit_utf8() {
	emit_utf8_ = true;
	return *this;
}
void Writer::writeValueFormatted(const Value& value) {
	switch (value.type()) {
	case Type::kNull:return writeNull();
	case Type::kBoolean:return writeBoolean(value);
	case Type::kNumber:return writeNumber(value);
	case Type::kString:return writeString(value.asString());
	case Type::kArray:return writeArrayFormatted(value);
	case Type::kObject:return writeObjectFormatted(value);
	}
}
void Writer::writeValue(const Value& value) {
	switch (value.type()) {
	case Type::kNull:return writeNull();
	case Type::kBoolean:return writeBoolean(value);
	case Type::kNumber:return writeNumber(value);
	case Type::kString:return writeString(value.asString());
	case Type::kArray:return writeArray(value);
	case Type::kObject:return writeObject(value);
	}
}
const std::string& Writer::getOutput()const {
	return out_;
}
void Writer::writeHex16Bit(unsigned u) {
	constexpr const char hex2[513] =
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
	const unsigned hi = (u >> 8) & 0xFF;
	const unsigned lo = u & 0xFF;
	out_ += hex2[2 * hi];
	out_ += hex2[2 * hi + 1];
	out_ += hex2[2 * lo];
	out_ += hex2[2 * lo + 1];
}
void Writer::writeIndent() {
	for (unsigned i = depth_of_indentation_; i; --i)
		out_ += JSON_INDENT;
}
void Writer::writeNull() {
	out_.append("null", 4);
}
void Writer::writeBoolean(const Value& value) {
	value.asBool() ? out_.append("true", 4) : out_.append("false", 5);
}
void Writer::writeNumber(const Value& value) {
	char buffer[21]{ 0 };
	std::to_chars(buffer, buffer + sizeof buffer, value.asNumber());
	out_.append(buffer);
}
void Writer::writeString(std::string_view str) {
	const char* cur = str.data();
	const char* end = cur + str.length();
	out_ += '"';
	while (cur < end) {
		char c = *cur;
		switch (c) {
		case '"':out_ += "\\\""; break;
		case '\\':out_ += "\\\\"; break;
		case '\b':out_ += "\\b"; break;
		case '\f':out_ += "\\f"; break;
		case '\n':out_ += "\\n"; break;
		case '\r':out_ += "\\r"; break;
		case '\t':out_ += "\\t"; break;
		default:
			if (emit_utf8_) {
				if (uint8_t(c) < 0x20) {
					out_ += "\\u";
					writeHex16Bit(c);
				}
				else {
					out_ += c;
				}
			}
			else {
				unsigned codepoint = UTF8ToCodepoint(cur, end); // modifies `c`
				if (codepoint < 0x20) {
					out_ += "\\u";
					writeHex16Bit(codepoint);
				}
				else if (codepoint < 0x80) {
					out_ += static_cast<char>(codepoint);
				}
				else if (codepoint < 0x10000) {
					// Basic Multilingual Plane
					out_ += "\\u";
					writeHex16Bit(codepoint);
				}
				else {
					// Extended Unicode. Encode 20 bits as a surrogate pair.
					codepoint -= 0x10000;
					out_ += "\\u";
					writeHex16Bit(0xD800 + ((codepoint >> 10) & 0x3FF));
					writeHex16Bit(0xDC00 + (codepoint & 0x3FF));
				}
				break;
			}
		}
		++cur;
	}
	out_ += '"';
}
void Writer::writeArray(const Value& value) {
	out_ += '[';
	if (!value.asArray().empty()) {
		for (auto& val : value.asArray()) {
			writeValue(val);
			out_ += ',';
		}
		out_.pop_back();
	}
	out_ += ']';

}
void Writer::writeObject(const Value& value) {
	out_ += '{';
	if (!value.asObject().empty()) {
		for (auto& [key, val] : value.asObject()) {
			writeString(key);
			out_ += ':';
			writeValue(val);
			out_ += ',';
		}
		out_.pop_back();
	}
	out_ += '}';
}
void Writer::writeArrayFormatted(const Value& value) {
	out_ += '[';
	if (!value.asArray().empty()) {
		out_ += '\n';
		++depth_of_indentation_;
		for (auto& val : value.asArray()) {
			writeIndent();
			writeValueFormatted(val);
			out_ += ",\n";
		}
		--depth_of_indentation_;
		out_.pop_back();
		out_.pop_back();
		out_ += '\n';
		writeIndent();
	}
	out_ += ']';
}
void Writer::writeObjectFormatted(const Value& value) {
	out_ += '{';
	if (!value.asObject().empty()) {
		out_ += '\n';
		++depth_of_indentation_;
		for (auto& [key, val] : value.asObject()) {
			writeIndent();
			writeString(key);
			out_ += ": ";
			writeValueFormatted(val);
			out_ += ",\n";
		}
		--depth_of_indentation_;
		out_.pop_back();
		out_.pop_back();
		out_ += '\n';
		writeIndent();
	}
	out_ += '}';
}

Value::Value() :
	data_(nullptr) {
}
Value::Value(nullptr_t) :
	data_(nullptr) {
}
Value::Value(bool value) :
	data_(value) {
}
Value::Value(double value) :
	data_(value) {
}
Value::Value(const char* value) :
	data_(value) {
}
Value::Value(const std::string& value) :
	data_(value) {
}
Value::Value(const Value& other) :
	data_(other.data_) {
	//switch (other.type_) {
	//case Type::kString:
	//	_setString(*other.data_.s);
	//	break;
	//case Type::kArray:
	//	_setArray(*other.data_.a);
	//	break;
	//case Type::kObject:
	//	_setObject(*other.data_.o);
	//	break;
	//default:
	//	data_ = other.data_;
	//	break;
	//}
};
Value::Value(Value&& other)noexcept :
	data_(other.data_) {
};
Value::~Value() {
	//switch (type_) {
	//case Type::kString:
	//	delete data_.s;
	//	break;
	//case Type::kArray:
	//	delete data_.a;
	//	break;
	//case Type::kObject:
	//	delete data_.o;
	//	break;
	//}
	//type_ = Type::kNull;
}
Value& Value::operator=(const Value& other) {
	return operator=(Value(other));
};
Value& Value::operator=(Value&& other)noexcept {
	swap(other);
	return *this;
};
bool Value::operator==(const Value& other)const {
	return data_ == other.data_;
}
Value& Value::operator[](const std::string& index) {
	JSON_ASSERT(isObject() || isNull(), "Value must be object or null");
	if (isNull())
		data_ = Object();
	return asObject()[index];
}
Value& Value::operator[](size_t index) {
	JSON_ASSERT(isArray() || isNull(), "Value must be array or null");
	if (isNull())
		data_ = Array();
	if (asArray().size() < index)
		Fatal("index out of range");
	return asArray()[index];
}
void Value::insert(const std::string& index, Value&& value) {
	JSON_ASSERT(isObject() || isNull(), "Value must be object or null");
	if (isNull())
		data_ = Object();
	asObject().emplace(index, value);
}
void Value::swap(Value& other) {
	data_.swap(other.data_);
}
bool Value::remove(const std::string& index) {
	if (isObject()) {
		asObject().erase(index);
		return true;
	}
	return false;
}
bool Value::remove(size_t index) {
	if (isArray() && index < size()) {
		asArray().erase(asArray().begin() + index);
		return true;
	}
	return false;
}
void Value::append(const Value& value) {
	append(Value(value));
}
void Value::append(Value&& value) {
	JSON_ASSERT(isArray() || isNull(), "Value must be array or null");
	if (isNull())
		data_ = Array();
	asArray().push_back(std::move(value));
}
size_t Value::size()const {
	switch (type()) {
	case Type::kString:
		return asString().size();
	case Type::kArray:
		return asArray().size();
	case Type::kObject:
		return asObject().size();
	default:
		break;
	}
	return 0;
}
bool Value::empty()const {
	switch (type()) {
	case Type::kNull:
		return true;
	case Type::kString:
		return asString().empty();
	case Type::kArray:
		return asArray().empty();
	case Type::kObject:
		return asObject().empty();
	default:
		break;
	}
	return false;
}
bool Value::contains(const std::string& key)const {
	if (isObject())
		return asObject().find(key) != asObject().end();
	return false;
}
void Value::clear() {
	switch (type()) {
	case Type::kString:
		asString().clear();
		break;
	case Type::kArray:
		asArray().clear();
		break;
	case Type::kObject:
		asObject().clear();
		break;
	default:
		break;
	}
	//type_ = Type::kNull;
}
std::string Value::dump()const {
	Writer w;
	w.writeValueFormatted(*this);
	return w.getOutput();
}
} // namespace Json

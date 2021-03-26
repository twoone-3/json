# Json
Convenient c++JSON library<br>
[简体中文](README.md) | [English](README_EN.md)
# Usage
Include `Json.h` in your project<br>
* Serialize string:
```cpp
const char* str = "{}";
Json::Value v = Json::toJson(v);
```
* Iterate over JSON objects
```cpp
Json::Value v(toJson(R"({"key":"value"})"));
for (auto& x : v.asObject())
	puts(x.second.toString().c_str());
```
* Value & Assignment:
```cpp
Json::Value v(toJson(R"({"key":"value"})"));
cout << j["key"].toString() << endl;
j["key"] = 0;
cout << j["key"].toString() << endl;
```
# Features
1. Each Json object only occupies 16 bytes
2. The entire library has only one header file
3. Support UTF-8 string parsing
4. Support error reporting when parsing
# Reference
https://github.com/jo-qzy/MyJson/
https://github.com/open-source-parsers/jsoncpp

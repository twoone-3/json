# Json
方便的c++JSON库<br>
[简体中文](README.md) | [English](README_EN.md)
# 用法
在你的项目包括`Json.h`<br>
* 序列化字符串:
```cpp
const char* str = "{}";
Json::Value v = Json::toJson(v);
```
* 遍历:
```cpp
Json::Value v(toJson(R"({"key":"value"})"));
for (auto& x : v.asObject())
	puts(x.second.toString().c_str());
```
* 取值&赋值:
```cpp
Json::Value v(toJson(R"({"key":"value"})"));
cout << j["key"].toString() << endl;
j["key"] = 0;
cout << j["key"].toString() << endl;
```
# 特点
1. 每个Json对象仅占用16字节
2. 整个库只有一个头文件
3. 支持UTF-8的字符串解析
4. 支持解析时报错
# 参考
https://github.com/jo-qzy/MyJson/
https://github.com/open-source-parsers/jsoncpp

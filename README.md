# json
使用 C++ 特性编写的 JSON 库  
[简体中文](README.md) | [English](README_EN.md)

# 前言
为什么要写这样一个JSON库？github上目前有很多大型的JSON解析库，如RapidJSON、jsoncpp、nlohmann_json，但由于历史等不可抗力原因，它们有着各自的缺点与不足，各种繁多的代码堆积，导致维护困难，我致力于打造一个代码最简单，速度快，便携的JSON库。

# 特点
1. 百般打磨的API
2. 使用std::variant进行操作，内存友好
3. 拥有比nlohmann稍高的效率
4. 代码遵循`Google C++ Style`

# 用法
1. 将`json.cpp`和`json.h`添加到你的项目
2. 将项目的语言标准调整到`c++17`或者更新的版本
3. 在你的`.c`或者`.cpp`文件包括`json.h`

# 文档
## 主要类
* Value
用于储存、查询和修改JSON数据
* Parser
用于把一个JSON字符串解析成Value
```cpp
Parser p;
Value v;
if (!p.parse(s, v))
	cout << p.getError() << endl;
```
* Writer
用于把Value对象序列化成JSON字符串
```cpp
Writer w;
Value v;
w.writeValue(v);
cout << w.getOutput() << endl;
```
当然你也可以用Value::dump()来实现此功能  
如果要配置序列化模式，就得用此例子

# 参考
https://github.com/jo-qzy/MyJson/
https://github.com/open-source-parsers/jsoncpp
https://github.com/nlohmann/json

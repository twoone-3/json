# Json
[![CodeFactor](https://www.codefactor.io/repository/github/twoone-3/json/badge)](https://www.codefactor.io/repository/github/twoone-3/json)
方便的c++JSON库<br>
[简体中文](README.md) | [English](README_EN.md)
# 特点
1. 简单易用
2. 内存友好
3. 效率高
4. 支持解析时报错
# 用法
1. 将json.cpp和json.h添加到你的项目
2. 将项目的语言标准调整到c++17
3. 包括json.h文件
4. 接下来就可以使用本json库了
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
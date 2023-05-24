# json
JSON library written with C++ features  
[简体中文](README.md) | [English](README_EN.md)

# Preface
Why write such a JSON library? github currently has a lot of large JSON parsing library, such as RapidJSON, jsoncpp, nlohmann_json, but due to history and other force majeure reasons, they have their own shortcomings and shortcomings, a variety of a wide range of code pile up, resulting in maintenance difficulties, I am committed to creating a code the most simple, fast, portable JSON library. Fast, portable JSON library.

# Features
1. a hundred polished API
2. use std::variant for operations, memory-friendly
3. has a slightly higher efficiency than nlohmann
4. The code follows `Google C++ Style`

# Usage
1. add `json.cpp` and `json.h` to your project
2. adjust the project's language standard to `c++17` or newer
3. Include `json.h` in your `.c` or `.cpp` file

# Documentation
## Main classes
* Value
Used to store, query and modify JSON data
* Parser
Used to parse a JSON string into Value
```cpp
Parser p.
Value v.
if (!p.parse(s, v))
	cout << p.getError() << endl.
```
* Writer
Used to serialize Value objects into JSON strings
```cpp
Writer w.
Value v.
w.writeValue(v).
cout << w.getOutput() << endl.
```
Of course you can also use Value::dump() for this function  
If you want to configure serialization mode, you have to use this example

# Reference
https://github.com/jo-qzy/MyJson/
https://github.com/open-source-parsers/jsoncpp
https://github.com/nlohmann/json


Translated with www.DeepL.com/Translator (free version)
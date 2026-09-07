# json
一个用 C++ 从零实现的 JSON 解析 / 序列化库。核心仅有 `json.h`（约 150 行）与 `json.cpp`（约 700 行）两个文件，无第三方依赖，在 Windows + Visual Studio（MSVC）下开发与验证。MIT 许可证。

[简体中文](README.md) | [English](README_EN.md)

## 项目简介

GitHub 上已有 nlohmann/json、jsoncpp、RapidJSON 等成熟的 JSON 库。本项目不追求功能上的大而全，而是从以下三个方面实现一个"小而清晰"的 JSON 库：

- **解析**：手写递归下降解析器，单个头文件 + 单个源文件完成；
- **存储**：用 `std::variant` 直接映射 JSON 的六种类型，类型安全、布局紧凑；
- **API**：`Value` / `Reader` / `Writer` 三个类分别负责数据存储、解析与序列化，几行代码即可上手。

> 说明：Unicode / UTF-8 相关处理参考了 jsoncpp 的实现思路（见文末"参考"），并非原创算法。

## 主要功能

- 支持 JSON 全部六种数据类型：`null`、`true/false`、数字、字符串、数组、对象
- 解析字符串 / 文件：`Reader::parse`、`Reader::parseFile`（自动跳过 UTF-8 BOM）
- 序列化：紧凑输出（`Writer::writeValue`）与美化输出（`Writer::writeValueFormatted`，缩进可配置），也可直接使用 `Value::dump()`
- 字符串转义与 `\uXXXX` 处理；`Writer::emit_utf8()` 可选输出 UTF-8 而非转义
- 可选的 `//` 与 `/* */` 注释扩展（`Reader::allowComments`，默认关闭）
- 解析失败返回 `false`，通过 `Reader::getError()` 获取带行号的错误信息（不抛异常）
- 对象以 `std::map` 存储，遍历顺序确定，序列化结果可复现

## 核心设计 / 代码结构

```
json.h     API 声明：Type 枚举、Value、Reader、Writer
json.cpp   实现：UTF-8 转换、递归下降解析器、Writer 序列化
main.cpp   命令行样例程序（遍历 test/ 并打印解析结果，非断言测试）
test/      JSON 测试样例（见"测试说明"）
```

### 数据模型：`Value` + `std::variant`

`Value` 的核心是一个 `std::variant`：

```cpp
using Array  = std::vector<Value>;
using Object = std::map<std::string, Value>;
using Data   = std::variant<nullptr_t, bool, double, std::string, Array, Object>;
```

- JSON 六种类型与 `variant` 的六个成员一一对应，标量 / 字符串直接内联存储，数组 / 对象的元素由 `vector` / `map` 组织在堆上；
- `Value::type()` 直接返回 `data_.index()`，无需额外保存类型标记；
- 统一通过 `std::get<T>(data_)` 取值，类型错误在取值时即可发现。

### 解析器：`Reader`

手写递归下降解析器，单遍扫描、无回溯：

- `parseString`：处理 `\"` `\\` `\/` `\b` `\f` `\n` `\r` `\t` 与 `\uXXXX`；高位代理（`0xD800`–`0xDBFF`）与低位代理成对出现时合并为完整码点后按 UTF-8 编码；
- `parseNumber`：通过 `std::from_chars` 转换为 `double`，超出范围（`result_out_of_range`）时报错；
- `parseArray` / `parseObject`：可解析任意深度嵌套的数组 / 对象；
- 错误处理：`bool` 返回值 + `getError()`（内含行号），不依赖异常机制。

### 序列化器：`Writer`

- `writeValue`：紧凑输出（无多余空白）；
- `writeValueFormatted`：美化输出，缩进字符串可配置（`Writer::indent`），默认 4 空格；
- `emit_utf8()`：非 ASCII 字符直接以 UTF-8 输出，而不是 `\uXXXX`；
- `Value::dump(emit_utf8, indent)` 为一行式便捷接口，内部即使用 `Writer`。

## 支持的 JSON 类型

| JSON 类型 | 内部表示 |
| --- | --- |
| `null` | `std::nullptr_t` |
| `true` / `false` | `bool` |
| 数字（统一按浮点存储） | `double` |
| 字符串（UTF-8） | `std::string` |
| 数组 | `std::vector<Value>` |
| 对象 | `std::map<std::string, Value>` |
## 解析与序列化能力

- **数字**：整数、小数、指数记法（如 `1e1`、`0.1e1`、`1e-1`、`1e00`、`2e-00`）均可解析；超出 `double` 范围（如 `1e400`）时报 "number out of range"。
- **字符串**：标准转义均可解析 / 生成；`\uXXXX` 按 UTF-8 编解码。
- **容错**：自动跳过 UTF-8 BOM；可选支持 `//` 与 `/* */` 注释；错误信息带行号。
- **序列化**：紧凑 / 美化两种格式；缩进与是否输出 UTF-8 可配置。

## 使用的 C++ 特性

工程配置为 C++17 / C++20（`x64` 配置显式设为 C++20），`json.h` 在编译期检查语言版本（要求 C++17 及以上）。实际使用的特性均为 ISO C++17 标准库 / 语言特性：

**标准库组件（C++17）**

- `std::variant`（`<variant>`）：C++17 引入的类型安全的可区分联合（discriminated union），本项目用它实现 `Value` 的存储层，将 JSON 的六种类型映射为六个成员类型
- `std::string_view`（`<string_view>`）：C++17 引入的非变异字符串视图，用作 `parse` / `indent` / `dump` 等接口的只读字符串参数，避免不必要的字符串复制
- `std::to_chars` / `std::from_chars`（`<charconv>`）：C++17 引入的字符序列与数值互转函数，支持最短往返（shortest round-trip）输出；本项目用 `std::to_chars` 序列化数字、用 `std::from_chars` 解析数字（其 `result_out_of_range` 用于报告数字溢出）
- `std::filesystem::*`（`<filesystem>`）：C++17 引入的文件系统库，`main.cpp` 用它遍历 `test/` 目录
- 结构化绑定 `auto& [key, val]`：对象序列化时遍历键值对
- `enum class`、模板、`std::map` / `std::vector` 等容器与 RAII 资源管理

## 测试说明

`test/` 目录包含 36 个测试样例（参考并重命名自 JSONTestSuite 系列样例）：

- `pass01.json` – `pass03.json`：合法 JSON
- `fail01.json` – `fail33.json`：非法 JSON
- `fail01_EXCLUDE.json`、`fail18_EXCLUDE.json`：套件标注为"实现相关 / 可选"的样例（顶层为字符串、过深嵌套），目前不作为判定依据

`main.cpp` 会遍历 `test/` 逐个解析并打印输出，同时统计总耗时（毫秒）。注意：**该程序不做断言**——它不会校验"合法样例必须解析成功、非法样例必须解析失败"，也未接入 CI。断言式测试是后续待完善项。

## 编译与运行（Visual Studio）

本项目使用 Visual Studio 工程文件（`Json.sln` / `json.vcxproj`），不提供 CMake：

1. 安装 Visual Studio（勾选 C/C++ 组件）后打开 `Json.sln`；
2. 在右上角选择配置 `json_test` → `x64` → `Debug`（或 `Release`），点击构建并运行（F5 / Ctrl+F5）；
3. 程序会在当前工作目录（仓库根目录）下扫描 `test/`，逐个解析并输出结果与总耗时。

工程配置说明：`x64` 配置将语言标准显式设为 C++20（`stdcpp20`）；`Win32` 配置使用 MSVC 默认语言版本；两种配置均可正常编译（要求 C++17 及以上）。

### 嵌入其他项目

1. 将 `json.cpp` 与 `json.h` 拷贝进你的工程；
2. 将项目语言标准设为 C++17 或更高；
3. 在源码中 `#include "json.h"` 即可使用。

## 简单使用示例

```cpp
#include <iostream>
#include "json.h"

int main() {
  // 1) 解析
  json::Reader reader;
  json::Value v;
  if (!reader.parse("{\"name\": \"json\", \"version\": 1, \"ok\": true}", v)) {
    std::cout << reader.getError() << '\n';
    return -1;
  }

  // 2) 查询与修改
  std::cout << v["name"].asString() << '\n';  // json
  v["version"] = 2;

  // 3) 紧凑序列化
  json::Writer w;
  w.writeValue(v);
  std::cout << w.getOutput() << '\n';
  // 输出: {"name":"json","version":2,"ok":true}

  // 4) 美化序列化：默认 4 空格缩进；也可指定 UTF-8 输出 + Tab 缩进
  std::cout << v.dump() << '\n';
  std::cout << v.dump(true, "\t") << '\n';

  // 5) 解析文件
  json::Value doc;
  if (!reader.parseFile("config.json", doc))
    std::cout << reader.getError() << '\n';

  return 0;
}
```

## 已知限制

- `json.cpp` 的 `Reader::parseString` 中，低位代理合并分支的条件判断疑似写反（`if (!parseHex4(surrogatePair))`），合法代理对可能被误判为字符错误；该问题尚未修复，代理对识别能力有待测试验证。
- `Reader::parse` 解析完顶层值后不检查剩余内容，例如解析 `"1 2"` 会成功并忽略尾随的 `2`。
- 项目目前仅在 Visual Studio / MSVC（Windows）下编译与运行验证，尚未在 gcc / clang 等其他工具链下实测。

## 参考

- https://github.com/jo-qzy/MyJson/
- https://github.com/open-source-parsers/jsoncpp（Unicode / UTF-8 相关实现参考）
- https://github.com/nlohmann/json

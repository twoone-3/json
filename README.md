# json
一个用 C++ 从零实现的 JSON 解析 / 序列化库。核心仅有 `json.h`（约 230 行）与 `json.cpp`（约 1200 行）两个文件，无第三方依赖。项目使用 C++17，并通过 GitHub Actions 在 Windows（MSVC）、Linux（GCC）和 macOS（GCC）上自动构建与测试。MIT 许可证。

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
- 整数按 64 位精确保留（`12345678901234567890` 往返后不变），小数与指数形式按 `double` 存储
- 严格校验 UTF-8：解析器拒绝非法 UTF-8 字节序列，序列化器把非法字节替换为 `U+FFFD`
- 限制嵌套深度（默认 512 层，`Reader::maxDepth()` 可调），避免超深输入导致栈溢出
- 可选的 `//` 与 `/* */` 注释扩展（`Reader::allowComments`，默认关闭）
- 解析失败返回 `false`，通过 `Reader::getError()` 获取带行号的错误信息（不抛异常）
- 对象以 `std::map` 存储，遍历顺序确定，序列化结果可复现

## 与 JSON 标准（RFC 8259）的关系

本库的解析器与序列化器以 [RFC 8259](https://www.rfc-editor.org/rfc/rfc8259) 和
[json.org](https://www.json.org/json-en.html) 为准，已通过官方
[JSONTestSuite](https://github.com/nst/JSONTestSuite) 全部 318 个用例的验证
（95 个 `y_` 合法用例全部解析成功、188 个 `n_` 非法用例全部被拒绝、序列化结果往返一致）。

需要注意的两点：

- **数字范围**：RFC 8259 §6 明确允许实现限制所接受数字的范围与精度，因此超出
  `double` 表示范围的数字（如 `1e400`、`1e-400`）会被拒绝并报 "number out of range"。
  64 位整数范围内的整数则是精确保留的。
- **文本编码**：RFC 8259 §8.2 要求 JSON 文本使用 UTF-8 编码，因此解析器会拒绝
  非法 UTF-8 字节序列（如 lone surrogate、overlong encoding、`> U+10FFFF` 的码点）。

## 核心设计 / 代码结构

```
json.h     API 声明：Type 枚举、Number、Value、Reader、Writer
json.cpp   实现：UTF-8 转换、递归下降解析器、Writer 序列化
main.cpp   测试程序（运行端到端样例、JSONTestSuite 和行为/回归测试并输出结果）
test/      JSON 测试样例（见"测试说明"）
```

### 数据模型：`Value` + `std::variant`

`Value` 的核心是一个 `std::variant`：

```cpp
using Array  = std::vector<Value>;
using Object = std::map<std::string, Value>;
using Data   = std::variant<nullptr_t, bool, Number, std::string, Array, Object>;
```

- JSON 六种类型与 `variant` 的六个成员一一对应，标量 / 字符串直接内联存储，数组 / 对象的元素由 `vector` / `map` 组织在堆上；
- `Value::type()` 直接返回 `data_.index()`，无需额外保存类型标记；
- 统一通过 `std::get<T>(data_)` 取值，类型错误在取值时即可发现。

### 数字：`Number`

数字由 `Number` 类表示，保留写入时的精确表示：

```cpp
class Number {
  enum class Kind : uint8_t { kDouble, kInt, kUInt };
};
```

- `kInt`：能放进 `int64_t` 的整数（`-9223372036854775808` – `9223372036854775807`）；
- `kUInt`：超过 `INT64_MAX` 但能放进 `uint64_t` 的整数；
- `kDouble`：小数、指数形式以及 `-0`。

整数按 64 位精确保留：`{"id":12345678901234567890}` 解析后再序列化仍是
`12345678901234567890`，每一位都不会丢失。
`Value::asDouble()` / `asInt()` / `asInt64()` 等接口提供有损转换，超出目标类型范围时
做饱和截断，不会触发未定义行为。

### 解析器：`Reader`

手写递归下降解析器，单遍扫描、无回溯：

- `parseString`：处理 `\"` `\\` `\/` `\b` `\f` `\n` `\r` `\t` 与 `\uXXXX`；高位代理（`0xD800`–`0xDBFF`）与低位代理成对出现时合并为完整码点后按 UTF-8 编码，孤立代理一律拒绝；同时校验原始字节是否为合法 UTF-8；
- `parseNumber`：先按 JSON 文法（`number = [ minus ] int [ frac ] [ exp ]`）逐字符扫描，再交给 `std::from_chars` 转换，因此 `1.`、`.5`、`+1`、`-Infinity`、`01` 等 JSON 之外的形式都会被拒绝；
- `parseArray` / `parseObject`：递归解析，嵌套深度超过 `Reader::maxDepth()`（默认 512）时返回错误，不会因超深输入耗尽栈；
- 所有缓冲区访问都先判边界，不会越界读；
- 错误处理：`bool` 返回值 + `getError()`（内含行号），不依赖异常机制。

### 序列化器：`Writer`

- `writeValue`：紧凑输出（无多余空白）；
- `writeValueFormatted`：美化输出，缩进字符串可配置（`Writer::indent`），默认 4 空格；
- `emit_utf8()`：非 ASCII 字符直接以 UTF-8 输出，而不是 `\uXXXX`；
- `NaN` / `Infinity` 没有 JSON 表示，一律输出 `null`；
- `Value::dump(emit_utf8, indent)` 为一行式便捷接口，内部即使用 `Writer`。

## 支持的 JSON 类型

| JSON 类型 | 内部表示 |
| --- | --- |
| `null` | `std::nullptr_t` |
| `true` / `false` | `bool` |
| 数字 | `Number`（`int64_t` / `uint64_t` / `double` 三选一，精确往返） |
| 字符串（UTF-8） | `std::string` |
| 数组 | `std::vector<Value>` |
| 对象 | `std::map<std::string, Value>` |

## 解析与序列化能力

- **数字**：整数、小数、指数记法（如 `1e1`、`0.1e1`、`1e-1`、`1e00`、`2e-00`）均可解析；整数在 64 位范围内精确保留；超出 `double` 范围（如 `1e400`）时报 "number out of range"。
- **字符串**：标准转义均可解析 / 生成；`\uXXXX` 按 UTF-8 编解码；孤立代理与非法 UTF-8 字节序列一律拒绝。
- **容错**：自动跳过 UTF-8 BOM；可选支持 `//` 与 `/* */` 注释；错误信息带行号。
- **安全性**：限制嵌套深度（默认 512），所有输入访问均已判边界，无越界读写。
- **序列化**：紧凑 / 美化两种格式；缩进与是否输出 UTF-8 可配置；输出始终是合法的 JSON 文档。

## 使用的 C++ 特性

工程配置为 C++17，`json.h` 在编译期检查语言版本（要求 C++17 及以上）。标准库还需提供浮点版本的 `std::to_chars` / `std::from_chars`（GCC ≥ 11、libc++ ≥ 14、MSVC ≥ 19.14）。实际使用的特性均为 ISO C++17 标准库 / 语言特性：

**标准库组件（C++17）**

- `std::variant`（`<variant>`）：C++17 引入的类型安全的可区分联合（discriminated union），本项目用它实现 `Value` 的存储层，将 JSON 的六种类型映射为六个成员类型
- `std::string_view`（`<string_view>`）：C++17 引入的非变异字符串视图，用作 `parse` / `indent` / `dump` 等接口的只读字符串参数，避免不必要的字符串复制
- `std::to_chars` / `std::from_chars`（`<charconv>`）：C++17 引入的字符序列与数值互转函数，支持最短往返（shortest round-trip）输出；本项目用 `std::to_chars` 序列化数字、用 `std::from_chars` 解析数字（其 `result_out_of_range` 用于报告数字溢出）
- `std::filesystem::*`（`<filesystem>`）：C++17 引入的文件系统库，`main.cpp` 用它遍历 `test/` 目录
- `std::is_integral_v` / `std::is_signed_v` / `std::enable_if_t`（`<type_traits>`）：`Value` 的整数构造函数模板，用来精确保留 64 位整数并消除 `Value(1)` 的重载歧义
- 结构化绑定 `auto& [key, val]`：对象序列化时遍历键值对
- `enum class`、模板、`std::map` / `std::vector` 等容器与 RAII 资源管理

## 测试说明

`test/` 目录包含两类测试数据：

### 1. `test/*.json`：端到端样例

`pass01.json` 是 json.org 官方测试套件中的 "pass1" 模式 —— 一份用齐了全部六种 JSON
类型、多层嵌套、转义与 Unicode 的文档，用来验证"整份文档"的解析与序列化。
逐个 JSON 构造的细粒度用例由下面的官方套件覆盖，因此这里只保留这一份。

### 2. `test/jsonsuite/*.json`：官方 JSONTestSuite 全量 318 个用例

直接复制自 [nst/JSONTestSuite](https://github.com/nst/JSONTestSuite) 的 `test_parsing/`
（公有领域），命名规则如下：

| 前缀 | 含义 | 本项目的期望行为 |
| --- | --- | --- |
| `y_` | 合法 JSON | 必须解析成功 |
| `n_` | 非法 JSON | 必须被拒绝 |
| `i_` | 与实现相关（implementation defined） | 两种行为都允许，仅统计不判定 |

### 测试程序

`main.cpp` 是项目自带的测试程序（编译产物为可执行的 `json_test`），运行时会执行三类检查：

- **端到端样例**：`test/` 下的每份文档都必须解析成功，且 `Value::dump()` 的结果能重新解析（往返一致）；
- **JSONTestSuite**：遍历 `test/jsonsuite/`，要求 95 个 `y_` 全部解析成功、188 个 `n_` 全部被拒绝，并检查 `Value::dump()` 的结果能被重新解析（往返一致性）；`i_` 只统计；
- **行为与回归测试**：只覆盖官方套件表达不了的部分 —— 本库的扩展（注释、嵌套深度上限、`emit_utf8`）、API 层面的行为（类型判断、数值转换、`getError()`、重复键），以及历史缺陷的回归覆盖（数字缓冲区溢出、`NaN`/`Infinity`、注释吞字符、越界读、栈溢出、非法 UTF-8 静默改写、整数精度丢失等）。JSON 语法本身的正误一律由官方套件负责，不在这里重复；
- **退出码**：任一检查失败即返回非零退出码，并打印 `N/M checks passed` 汇总。

当前结果：**72/72 checks passed**（其中 JSONTestSuite：95/95 `y_`、188/188 `n_`、0 个往返失败）。

GitHub Actions（`.github/workflows/ci.yml`）会在 **Windows（MSVC）/ Linux（GCC）/ macOS（GCC）** 三个平台自动编译并运行上述测试，并在 push 到 `master` 或创建 Pull Request 时触发。

## 已知限制

以下行为是有意为之，其中前两条由 RFC 8259 明确允许：

1. **数字范围**：`double` 无法表示的数字（如 `1e400`、`1e-400`）会被拒绝（RFC 8259 §6 允许实现限制数字的范围与精度）。nlohmann/json 的行为与此一致。
2. **数字精度**：超出 `int64_t` / `uint64_t` 的整数（如 `99999999999999999999`）同样会被拒绝。
3. **对象键顺序**：对象用 `std::map` 存储，序列化时按键的字典序输出，与输入顺序无关。
4. **重复键**：`{"a":1,"a":2}` 中后一个值生效（与多数 JSON 库一致）。
5. **注释扩展**：默认关闭，需要显式调用 `Reader::allowComments()`；开启注释后，输入不满足严格合法的 JSON 语法。
6. **类型不匹配**：`asInt()` / `asString()` 等在类型不匹配时抛出 `std::bad_variant_access`（由 `std::get` 抛出），而不是返回默认值。

## 编译与运行（Visual Studio）

本项目使用 Visual Studio 工程文件（`Json.sln` / `json.vcxproj`），不提供 CMake：

1. 安装 Visual Studio（勾选 C/C++ 组件）后打开 `Json.sln`；
2. 在右上角选择配置 `json_test` → `x64` → `Debug`（或 `Release`），点击构建并运行（F5 / Ctrl+F5）；
3. 程序会在当前工作目录（仓库根目录）下扫描 `test/` 并运行测试，打印 `N/M checks passed` 汇总（任一检查失败返回非零退出码）。

工程配置说明：`Debug` / `Release` × `Win32` / `x64` 四个配置均将语言标准设为 C++17（`stdcpp17`），因此都能直接编译运行。

> **源码编码**：源文件以 UTF-8 编码保存。MSVC 默认按系统代码页读取源码，
> 因此工程文件已为所有配置加上 `/utf-8` 选项；用命令行编译时请同样加上
> `cl /std:c++17 /EHsc /utf-8 json.cpp main.cpp`（GCC / Clang 默认即为 UTF-8，无需额外选项）。

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

## 参考

- https://github.com/jo-qzy/MyJson/
- https://github.com/open-source-parsers/jsoncpp （Unicode / UTF-8 相关实现参考）
- https://github.com/nlohmann/json
- https://www.json.org/json-en.html
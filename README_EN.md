# json
A JSON parsing / serialization library written in C++ from scratch. The core consists of just two files — `json.h` (~150 LOC) and `json.cpp` (~700 LOC) — with no third-party dependencies. Developed and verified on Windows with Visual Studio (MSVC). MIT License.

[简体中文](README.md) | [English](README_EN.md)

## Introduction

There are already mature JSON libraries like nlohmann/json, jsoncpp and RapidJSON. This project does not aim at feature completeness; instead, it implements a small, clear JSON library with three goals:

- **Parsing**: a hand-written recursive-descent parser in a single header + source pair;
- **Storage**: the six JSON types are mapped 1:1 onto a `std::variant` — type-safe and compact;
- **API**: three classes — `Value` (storage), `Reader` (parsing), `Writer` (serialization) — get you started in a few lines of code.

> Unicode / UTF-8 related implementation draws on jsoncpp (see "Reference"); it is not an original algorithm.

## Features

- All six JSON types: `null`, `true` / `false`, number, string, array, object
- Parse from a string (`Reader::parse`) or a file (`Reader::parseFile`, skips UTF-8 BOM)
- Serialize compactly (`Writer::writeValue`) or pretty-printed (`Writer::writeValueFormatted`, configurable indent); or simply use `Value::dump()`
- Escape sequences and `\uXXXX`; `Writer::emit_utf8()` can emit UTF-8 instead of escapes
- Optional `//` and `/* */` comments (`Reader::allowComments`, off by default)
- On failure, `parse` returns `false` and `Reader::getError()` reports an error with the line number (no exceptions)
- Objects are stored in a `std::map` → deterministic iteration / serialization order

## Core design / file layout

```
json.h     API declarations: Type enum, Value, Reader, Writer
json.cpp   Implementation: UTF-8 conversion, recursive-descent parser, Writer
main.cpp   Command-line sample: iterates test/ and prints results (no assertions)
test/      JSON test cases (see "Testing")
```

### Data model: `Value` + `std::variant`

The core member of `Value` is a `std::variant`:

```cpp
using Array  = std::vector<Value>;
using Object = std::map<std::string, Value>;
using Data   = std::variant<nullptr_t, bool, double, std::string, Array, Object>;
```

- The six JSON types map 1:1 onto the six members of the variant; scalars / strings are stored inline, while array / object elements live on the heap (`vector` / `map`);
- `Value::type()` simply returns `data_.index()` — no extra type tag is kept;
- Values are accessed through `std::get<T>(data_)` with type checking.

### Parser: `Reader`

Hand-written recursive descent, single pass, no backtracking:

- `parseString`: handles `\"` `\\` `\/` `\b` `\f` `\n` `\r` `\t` and `\uXXXX`; high surrogates (`0xD800`–`0xDBFF`) followed by a low surrogate are merged into one code point and encoded as UTF-8;
- `parseNumber`: `std::from_chars` converts the characters to a `double`; `result_out_of_range` is reported as an error;
- `parseArray` / `parseObject`: arbitrarily nested arrays / objects;
- Error handling: `bool` return value + `getError()` (with line number), no exceptions.

### Serializer: `Writer`

- `writeValue`: compact output (no extra whitespace);
- `writeValueFormatted`: pretty output; the indent string is configurable via `Writer::indent` (4 spaces by default);
- `emit_utf8()`: emit non-ASCII characters as UTF-8 instead of `\uXXXX`;
- `Value::dump(emit_utf8, indent)` is a one-line convenience built on top of `Writer`.

## Supported JSON types

| JSON type | Internal representation |
| --- | --- |
| `null` | `std::nullptr_t` |
| `true` / `false` | `bool` |
| number (stored as floating point) | `double` |
| string (UTF-8) | `std::string` |
| array | `std::vector<Value>` |
| object | `std::map<std::string, Value>` |
## Parsing & serialization capabilities

- **Numbers**: integer, decimal and exponential forms (e.g. `1e1`, `0.1e1`, `1e-1`, `1e00`, `2e-00`) are accepted; values out of `double` range (e.g. `1e400`) are rejected with "number out of range".
- **Strings**: all standard escapes are parsed / emitted; `\uXXXX` is encoded / decoded with UTF-8.
- **Tolerance**: UTF-8 BOM is skipped automatically; `//` and `/* */` comments are optional; errors carry a line number.
- **Serialization**: compact / pretty formats; configurable indent and UTF-8 output.

## C++ features used

The project builds with C++17 (the `x64` configuration sets `stdcpp17`); `json.h` checks the language version at compile time (C++17 or newer). Everything used here is standard ISO C++17:

**Standard library components (C++17)**

- `std::variant` (`<variant>`): the type-safe discriminated union introduced in C++17; used as the storage layer of `Value`, mapping the six JSON types onto six member types
- `std::string_view` (`<string_view>`): the non-mutating string view introduced in C++17; used as a read-only string parameter for `parse` / `indent` / `dump` to avoid copying strings
- `std::to_chars` / `std::from_chars` (`<charconv>`): C++17 functions converting between character sequences and numbers, with shortest round-trip output; `std::to_chars` serializes numbers and `std::from_chars` parses them (its `result_out_of_range` reports numeric overflow)
- `std::filesystem::*` (`<filesystem>`): the C++17 filesystem library; `main.cpp` uses it to iterate over `test/`
- Structured bindings `auto& [key, val]`: used while serializing objects
- `enum class`, templates, `std::map` / `std::vector` containers and RAII resource management

## Testing

`test/` contains 36 cases (renamed from JSONTestSuite-style cases):

- `pass01.json` – `pass03.json`: valid JSON
- `fail01.json` – `fail33.json`: invalid JSON
- `fail01_EXCLUDE.json`, `fail18_EXCLUDE.json`: cases marked "implementation-defined / optional" (a top-level string, an overly deep nesting); not used as pass / fail criteria

`main.cpp` is an assertion-based test program (built as an executable `json_test`). When run, it performs two kinds of checks:

- **File suite**: asserts that every `pass*` file in `test/` parses, and every `fail*` file (except `_EXCLUDE`) is rejected;
- **Unit tests**: cover basic types, escapes, the `\uD83D\uDE00` surrogate pair, lone-surrogate rejection, trailing-content rejection (e.g. `"1 2"`), number syntax (leading zeros), `dump` round-trip, etc.;
- **Exit code**: any failed assertion makes the program return a non-zero exit code, and it prints an `N/M checks passed` summary.

The project is built and tested automatically on **Windows (MSVC), Linux (GCC) and macOS (GCC)** via GitHub Actions (`.github/workflows/ci.yml`).

## Build & run (Visual Studio)

No CMake — this project is a Visual Studio solution (`Json.sln` / `json.vcxproj`):

1. Install Visual Studio with the C/C++ components and open `Json.sln`;
2. Select `json_test` → `x64` → `Debug` (or `Release`), then build and run (F5 / Ctrl+F5);
3. The program scans `test/` under the working directory (the repo root) and runs the assertions, printing an `N/M checks passed` summary (a non-zero exit code is returned if any assertion fails).

The `x64` configurations set the language standard to C++17 (`stdcpp17`); the `Win32` configurations use the MSVC default. Both require C++17 or newer.

### Using it in your own project

1. Copy `json.cpp` and `json.h` into your project;
2. Set the language standard to C++17 or newer;
3. `#include "json.h"` and start using it.

## Example

```cpp
#include <iostream>
#include "json.h"

int main() {
  // 1) Parse
  json::Reader reader;
  json::Value v;
  if (!reader.parse("{\"name\": \"json\", \"version\": 1, \"ok\": true}", v)) {
    std::cout << reader.getError() << '\n';
    return -1;
  }

  // 2) Query & modify
  std::cout << v["name"].asString() << '\n';  // json
  v["version"] = 2;

  // 3) Compact serialization
  json::Writer w;
  w.writeValue(v);
  std::cout << w.getOutput() << '\n';
  // {"name":"json","version":2,"ok":true}

  // 4) Pretty print: 4 spaces by default; you may also emit UTF-8 + tab indent
  std::cout << v.dump() << '\n';
  std::cout << v.dump(true, "\t") << '\n';

  // 5) Parse a file
  json::Value doc;
  if (!reader.parseFile("config.json", doc))
    std::cout << reader.getError() << '\n';

  return 0;
}
```

## Reference

- https://github.com/jo-qzy/MyJson/
- https://github.com/open-source-parsers/jsoncpp (reference for Unicode / UTF-8 handling)
- https://github.com/nlohmann/json
# json
A JSON parsing / serialization library written in C++ from scratch. The core consists of just two files — `json.h` (~230 LOC) and `json.cpp` (~1200 LOC) — with no third-party dependencies. The project uses C++17 and is automatically built and tested by GitHub Actions on Windows (MSVC), Linux (GCC), and macOS (GCC). MIT License.

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
- Integers are kept exactly as 64-bit values (`12345678901234567890` survives a round trip); fractions are stored as `double`
- Strict UTF-8 validation: the reader rejects malformed UTF-8 byte sequences, the writer replaces invalid bytes with `U+FFFD`
- Bounded nesting depth (512 by default, tunable via `Reader::maxDepth()`) so that deeply nested input cannot exhaust the stack
- Optional `//` and `/* */` comments (`Reader::allowComments`, off by default)
- On failure, `parse` returns `false` and `Reader::getError()` reports an error with the line number (no exceptions)
- Objects are stored in a `std::map` → deterministic iteration / serialization order

## Relation to the JSON specification (RFC 8259)

The parser and the serializer follow [RFC 8259](https://www.rfc-editor.org/rfc/rfc8259)
and [json.org](https://www.json.org/json-en.html). The implementation is verified
against the official [JSONTestSuite](https://github.com/nst/JSONTestSuite) — all
318 cases pass (95 `y_` cases parse, 188 `n_` cases are rejected, and every
serialized document re-parses to an equal value).

Two points deserve a note:

- **Range of numbers**: RFC 8259 §6 explicitly allows implementations to limit the
  range and the precision of the numbers they accept, so numbers that `double`
  cannot represent (`1e400`, `1e-400`, …) are rejected with "number out of range".
  Integers within the 64-bit range are kept exactly.
- **Text encoding**: RFC 8259 §8.2 requires JSON text to be encoded in UTF-8, so
  the reader rejects invalid UTF-8 byte sequences (lone surrogates, overlong
  encodings, code points above `U+10FFFF`, …).

## Core design / file layout

```
json.h     API declarations: Type enum, Number, Value, Reader, Writer
json.cpp   Implementation: UTF-8 conversion, recursive-descent parser, Writer
main.cpp   Test program: runs the end-to-end documents, the JSONTestSuite and the behaviour/regression tests
test/      JSON test cases (see "Testing")
```

### Data model: `Value` + `std::variant`

The core member of `Value` is a `std::variant`:

```cpp
using Array  = std::vector<Value>;
using Object = std::map<std::string, Value>;
using Data   = std::variant<nullptr_t, bool, Number, std::string, Array, Object>;
```

- The six JSON types map 1:1 onto the six members of the variant; scalars / strings are stored inline, while array / object elements live on the heap (`vector` / `map`);
- `Value::type()` simply returns `data_.index()` — no extra type tag is kept;
- Values are accessed through `std::get<T>(data_)` with type checking.

### Numbers: `Number`

Numbers are no longer stored as a plain `double`; they are wrapped in a `Number`
class that preserves the exact representation they were written in:

```cpp
class Number {
  enum class Kind : uint8_t { kDouble, kInt, kUInt };
};
```

- `kInt`: an integer that fits into `int64_t` (`-9223372036854775808` … `9223372036854775807`);
- `kUInt`: an integer above `INT64_MAX` that fits into `uint64_t`;
- `kDouble`: fractions, exponent forms and `-0`.

Consequently `{"id":12345678901234567890}` still serializes to
`12345678901234567890` instead of being silently rewritten to `1.2345678901234567e+19`.
`Value::asDouble()` / `asInt()` / `asInt64()` keep working as before and saturate
instead of invoking undefined behaviour when the value does not fit.

### Parser: `Reader`

Hand-written recursive descent, single pass, no backtracking:

- `parseString`: handles `\"` `\\` `\/` `\b` `\f` `\n` `\r` `\t` and `\uXXXX`; high surrogates (`0xD800`–`0xDBFF`) followed by a low surrogate are merged into one code point and encoded as UTF-8, lone surrogates are rejected; raw bytes are validated to be well-formed UTF-8;
- `parseNumber`: the JSON grammar (`number = [ minus ] int [ frac ] [ exp ]`) is scanned character by character before `std::from_chars` is used, so forms outside JSON (`1.`, `.5`, `+1`, `-Infinity`, `01`, …) are rejected;
- `parseArray` / `parseObject`: recursive parsing; input nested deeper than `Reader::maxDepth()` (512 by default) is reported as an error instead of exhausting the stack;
- every buffer access is bounds-checked — there are no out-of-bounds reads;
- Error handling: `bool` return value + `getError()` (with line number), no exceptions.

### Serializer: `Writer`

- `writeValue`: compact output (no extra whitespace);
- `writeValueFormatted`: pretty output; the indent string is configurable via `Writer::indent` (4 spaces by default);
- `emit_utf8()`: emit non-ASCII characters as UTF-8 instead of `\uXXXX`;
- `NaN` / `Infinity` have no JSON representation and are written as `null`;
- `Value::dump(emit_utf8, indent)` is a one-line convenience built on top of `Writer`.

## Supported JSON types

| JSON type | Internal representation |
| --- | --- |
| `null` | `std::nullptr_t` |
| `true` / `false` | `bool` |
| number | `Number` (either `int64_t`, `uint64_t` or `double` — exact round trip) |
| string (UTF-8) | `std::string` |
| array | `std::vector<Value>` |
| object | `std::map<std::string, Value>` |

## Parsing & serialization capabilities

- **Numbers**: integer, decimal and exponential forms (e.g. `1e1`, `0.1e1`, `1e-1`, `1e00`, `2e-00`) are accepted; integers within the 64-bit range are kept exactly; values out of `double` range (e.g. `1e400`) are rejected with "number out of range".
- **Strings**: all standard escapes are parsed / emitted; `\uXXXX` is encoded / decoded with UTF-8; lone surrogates and malformed UTF-8 byte sequences are rejected.
- **Tolerance**: UTF-8 BOM is skipped automatically; `//` and `/* */` comments are optional; errors carry a line number.
- **Safety**: the nesting depth is bounded (512 by default) and every input access is bounds-checked — there is no out-of-bounds read or write.
- **Serialization**: compact / pretty formats; configurable indent and UTF-8 output; the output is always a valid JSON document.

## C++ features used

The project builds with C++17; `json.h` checks the language version at compile time (C++17 or newer). The standard library must additionally provide the floating-point overloads of `std::to_chars` / `std::from_chars` (GCC ≥ 11, libc++ ≥ 14, MSVC ≥ 19.14). Everything used here is standard ISO C++17:

**Standard library components (C++17)**

- `std::variant` (`<variant>`): the type-safe discriminated union introduced in C++17; used as the storage layer of `Value`, mapping the six JSON types onto six member types
- `std::string_view` (`<string_view>`): the non-mutating string view introduced in C++17; used as a read-only string parameter for `parse` / `indent` / `dump` to avoid copying strings
- `std::to_chars` / `std::from_chars` (`<charconv>`): C++17 functions converting between character sequences and numbers, with shortest round-trip output; `std::to_chars` serializes numbers and `std::from_chars` parses them (its `result_out_of_range` reports numeric overflow)
- `std::filesystem::*` (`<filesystem>`): the C++17 filesystem library; `main.cpp` uses it to iterate over `test/`
- `std::is_integral_v` / `std::is_signed_v` / `std::enable_if_t` (`<type_traits>`): the templated integer constructor of `Value`, which keeps 64-bit integers exact and removes the overload ambiguity of `Value(1)`
- Structured bindings `auto& [key, val]`: used while serializing objects
- `enum class`, templates, `std::map` / `std::vector` containers and RAII resource management

## Testing

The `test/` directory holds two kinds of test data:

### 1. `test/*.json`: end-to-end document

`pass01.json` is the "pass1" pattern from the official json.org test suite — a single
document that uses all six JSON types, nested arrays / objects, escapes and Unicode
at once, so that a whole document can be parsed and serialized. The fine-grained
per-construct cases are covered by the official suite below, so this is the only one
kept here.

### 2. `test/jsonsuite/*.json`: all 318 official JSONTestSuite cases

Copied verbatim from the `test_parsing/` directory of
[nst/JSONTestSuite](https://github.com/nst/JSONTestSuite) (public domain). The
naming convention is:

| Prefix | Meaning | Expected behaviour here |
| --- | --- | --- |
| `y_` | valid JSON | must parse |
| `n_` | invalid JSON | must be rejected |
| `i_` | implementation defined | either is fine — counted only |

### Test program

`main.cpp` is the project's built-in test program (built as an executable `json_test`). When run, it performs three kinds of checks:

- **End-to-end documents**: every document in `test/` must parse, and `Value::dump()` must re-parse to an equal value (round-trip);
- **JSONTestSuite**: walks `test/jsonsuite/`, requires all 95 `y_` cases to parse and all 188 `n_` cases to be rejected, and verifies that `Value::dump()` can be re-parsed (round-trip); `i_` cases are only counted;
- **Behaviour & regression tests**: only cover what the official suite cannot express — this library's extensions (comments, the nesting-depth limit, `emit_utf8`), API-level behaviour (type predicates, numeric conversion, `getError()`, duplicate keys), and the defects that were reported (number buffer overflow, `NaN` / `Infinity`, comments eating a character, out-of-bounds reads, stack overflow, silent rewriting of invalid UTF-8, loss of integer precision, …). Whether a document is valid JSON at all is left to the official suite and is not repeated here;
- **Exit code**: any failed check makes the program return a non-zero exit code, and it prints an `N/M checks passed` summary.

Current result: **72/72 checks passed** (JSONTestSuite: 95/95 `y_`, 188/188 `n_`, 0 round-trip failures).

GitHub Actions (`.github/workflows/ci.yml`) automatically builds and runs these tests on **Windows (MSVC), Linux (GCC), and macOS (GCC)**. The workflow runs on pushes to `master` and on pull requests.

## Known limitations

The following behaviour is intentional; the first two are explicitly permitted by RFC 8259:

1. **Range of numbers**: numbers that `double` cannot represent (`1e400`, `1e-400`, …) are rejected (RFC 8259 §6 allows implementations to limit the range and precision of numbers). nlohmann/json behaves the same way.
2. **Number precision**: integers outside `int64_t` / `uint64_t` (e.g. `99999999999999999999`) are rejected as well.
3. **Key order**: objects are stored in a `std::map`, so serialization emits the keys in lexicographic order, independent of the input order.
4. **Duplicate keys**: in `{"a":1,"a":2}` the last value wins (like most JSON libraries).
5. **Comment extension**: disabled by default; `Reader::allowComments()` has to be called explicitly, and with comments enabled the input is no longer strictly valid JSON.
6. **Type mismatch**: `asInt()` / `asString()` and friends throw `std::bad_variant_access` (from `std::get`) on a type mismatch instead of returning a default value.

## Build & run (Visual Studio)

No CMake — this project is a Visual Studio solution (`Json.sln` / `json.vcxproj`):

1. Install Visual Studio with the C/C++ components and open `Json.sln`;
2. Select `json_test` → `x64` → `Debug` (or `Release`), then build and run (F5 / Ctrl+F5);
3. The program scans `test/` under the working directory (the repo root) and runs the tests, printing an `N/M checks passed` summary (a non-zero exit code is returned if any check fails).

All four configurations (`Debug` / `Release` × `Win32` / `x64`) set the language standard to C++17 (`stdcpp17`), so they all build and run out of the box.

> **Source encoding**: the sources are stored as UTF-8. MSVC reads
> source files using the system code page by default, so the project file adds
> `/utf-8` to every configuration; add the same switch when compiling from the
> command line (`cl /std:c++17 /EHsc /utf-8 json.cpp main.cpp`). GCC / Clang
> assume UTF-8 already and need no extra option.

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
- https://www.json.org/json-en.html
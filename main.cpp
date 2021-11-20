#pragma execution_character_set("utf-8")
#include <filesystem>
#include <fstream>
#include <chrono>
#include "json.h"

using namespace std;
using namespace json;

static constexpr auto testjson =
R"(
[
    "JSON Test Pattern pass1",
    {"object with 1 member":["array with 1 element"]},
    {},
    [],
    -42,
    true,
    false,
    null,
    {
        "integer": 1234567890,
        "real": -9876.543210,
        "e": 0.123456789e-12,
        "E": 1.234567890E+34,
        "":  23456789012E66,
        "zero": 0,
        "one": 1,
        "space": " ",
        "quote": "\"",
        "backslash": "\\",
        "controls": "\b\f\n\r\t",
        "slash": "/ & \/",
        "alpha": "abcdefghijklmnopqrstuvwyz",
        "ALPHA": "ABCDEFGHIJKLMNOPQRSTUVWYZ",
        "digit": "0123456789",
        "0123456789": "digit",
        "special": "`1~!@#$%^&*()_+-={':[,]}|;.</>?",
        "hex": "\u0123\u4567\u89AB\uCDEF\uabcd\uef4A",
        "true": true,
        "false": false,
        "null": null,
        "array":[  ],
        "object":{  },
        "address": "50 St. James Street",
        "url": "http://www.JSON.org/",
        "comment": "// /* <!-- --",
        "# -- --> */": " ",
        " s p a c e d " :[1,2 , 3

,

4 , 5        ,          6           ,7        ],"compact":[1,2,3,4,5,6,7],
        "jsontext": "{\"object with 1 member\":[\"array with 1 element\"]}",
        "quotes": "&#34; \u0022 %22 0x22 034 &#x22;",
        "\/\\\"\uCAFE\uBABE\uAB98\uFCDE\ubcda\uef4A\b\f\n\r\t`1~!@#$%^&*()_+-=[]{}|;:',./<>?"
: "A key can be any string"
    },
    0.5 ,98.6
,
99.44
,

1066,
1e1,
0.1e1,
1e-1,
1e00,2e+00,2e-00
,"rosebud"])";
static void init() {
	using namespace filesystem;
	using namespace std::chrono;
	auto start = system_clock::now();

	for (auto& x : directory_iterator("test")) {
		cout << "Test file: " << x << endl;
		//cout << "Source file:\n" << s << endl;
		Reader p;
		Value v;
		//if (!p.parseFile(testjson, v))
		//	cout << p.getError() << endl;
		if (!p.parseFile(x.path().string(), v))
			cout << p.getError() << endl;
		cout << "After parse:\n" << v << "\n\n";
	}

	auto end = system_clock::now();
	auto duration = duration_cast<milliseconds>(end - start);
	cout << "Cost " << duration.count() << "ms" << endl;
}
int main() {
	system("chcp 65001");
	init();
	return 0;
}

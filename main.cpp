#pragma execution_character_set("utf-8")
#include <filesystem>
#include <fstream>
#include <chrono>
#include "json.h"

using namespace std;
using namespace json;

static constexpr auto testjson =
R"({
	"az": {
		"key":"value\n"
	},
	"edf":0.2,
	"edef":"\u3467АЁет",
	"wes":18446744073709551615
}
)";
static void init() {
	using namespace filesystem;
	using namespace std::chrono;
	auto start = system_clock::now();

	for (auto& x : directory_iterator("test")) {
		cout << "Test file: " << x << endl;
		ifstream f(x);
		string s(istreambuf_iterator<char>(f), {});
		cout << "Source file:\n" << s << endl;
		Parser p;
		Value v;
		if (!p.parse(s, v))
			cout << p.getError() << endl;
		cout << "After parse:\n" << v << "\n\n";
	}

	auto end = system_clock::now();
	auto duration = duration_cast<milliseconds>(end - start);
	cout << "Cost" << duration.count() << "ms" << endl;
}
int main() {
	system("chcp 65001");
	init();
	return 0;
}

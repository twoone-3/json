#pragma execution_character_set("utf-8")
#include <filesystem>
#include <fstream>
#include <ctime>
#include "json.h"

using namespace std;

constexpr auto testjson = R"(
{
	"az": {
		"key":"value\n"
	},
	"edf":2.0,
	"edef":58273637338730.58749240958157003243567,
	"wes":5058e100
}
)";

int main() {
	using namespace json;
	using namespace filesystem;
	system("chcp 65001");
	clock_t start, end;
	start = clock();
	//¡­calculating¡­
	for (auto& x : directory_iterator("test")) {
		cout << x << endl;
		ifstream f(x);
		string s(istreambuf_iterator<char>(f), {});
		Value value;
		Value::Parser p;
		if (!p.parse(s, value))
			cerr << p.getError() << endl;
		cout << value << endl;
	}

	for (unsigned i = 0; i != 1; ++i) {
		Value value;
		Value::Parser p;
		if (!p.parse(testjson, value))
			cerr << p.getError() << endl;
		cout << value << endl;
	}
	end = clock();
	printf("Ö´ÐÐºÄÊ±%lfs\n", static_cast<double>(end - start) / CLK_TCK);
	return 0;
}

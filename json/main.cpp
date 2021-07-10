#pragma execution_character_set("utf-8")
#include <filesystem>
#include <fstream>
#include <Windows.h>
#include "json.h"

using namespace std;

constexpr auto testjson = R"(
{
	"az": {
		"key":"value\n"
	},
	"edf":2.8,
	"edef":58273637338730.58749240958157003243567,
	"wes":5058e100
}
)";

int main() {
	using namespace json;
	using namespace filesystem;
	system("chcp 65001");
	UINT64 start = GetTickCount64();
	for (auto& x : directory_iterator("test")) {
		//cout << x << endl;
		ifstream f(x);
		string s(ifstream::_Iter(f), {});
		Value value;
		value.parse(s);
		cout << value << endl;
	}
	UINT64 end = GetTickCount64();

	cout << end - start << "ms" << endl;
	system("pause");
	return 0;
}

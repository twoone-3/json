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
	"edef":"\u3467啊这",
	"wes":5058e100
}
)";

int main() {
	using namespace json;
	using namespace filesystem;
	system("chcp 65001");
	clock_t start, end;
	start = clock();
	//…calculating…
	for (auto& x : directory_iterator("test")) {
		cout << "测试文件：" << x << endl;
		ifstream f(x);
		string s(istreambuf_iterator<char>(f), {});
		cout << "原文件：\n" << s << endl;
		Value value(Parse(s));
		cout << "解析JSON后：\n" << value << "\n\n";
	}

	for (unsigned i = 0; i != 1; ++i) {
		Value value = {	
			Parse(testjson)
		};
		cout << value << endl;
		cout << Parse(value) << endl;
	}
	end = clock();
	printf("执行耗时%lfs\n", static_cast<double>(end - start) / CLK_TCK);
	return 0;
}

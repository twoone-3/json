#pragma execution_character_set("utf-8")
#include <filesystem>
#include <fstream>
#include <chrono>
#include "json.h"
//#include "json.hpp"

using namespace std;
using namespace json;

constexpr auto testjson = R"(
{
	"az": {/* 
		"key":"value\n"*/
	},
	"edf":0.2,
	"edef":"\u3467啊这",//注释
	"wes":5058e100
}
)";
static void init() {
	using namespace filesystem;
	using namespace std::chrono;
	auto start = system_clock::now();
#if 1
	for (auto& x : directory_iterator("test")) {
		cout << "测试文件：" << x << endl;
		ifstream f(x);
		string s(istreambuf_iterator<char>(f), {});
		cout << "原文件：\n" << s << endl;
		string err;
		Value value(Parse(testjson, &err, true));
		cout << err << endl;
		cout << "解析JSON后：\n" << value.dump(4, true) << "\n\n";
	}
#else
	for (auto& x : directory_iterator("test")) {
		//cout << "测试文件：" << x << endl;
		ifstream f(x);
		//string s(istreambuf_iterator<char>(f), {});
		//cout << "原文件：\n" << s << endl;
		try {
			nlohmann::json value(nlohmann::json::parse(f));
			value.dump(4);
		}
		catch (const std::exception& e) {
			cerr << e.what() << endl;
		}
		//cout << "解析JSON后：\n" << value.dump(4) << "\n\n";
	}
#endif

	auto end = system_clock::now();
	auto duration = duration_cast<microseconds>(end - start);
	cout << "花费了" << duration.count() << "微秒" << endl;
}
int main() {

	system("chcp 65001");
	init();
	return 0;
}

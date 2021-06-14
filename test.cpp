#pragma execution_character_set("utf-8")
#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include <chrono>
#include "json.h"
#define file 0

using namespace std;
using namespace Json;
//// 转换字符串为Json对象
//static Value toJson(const string& str) {
//	Value value;
//	CharReaderBuilder rb;
//	string errs;
//	CharReader* r(rb.newCharReader());
//	if (!r->parse(str.c_str(), str.c_str() + str.length(), &value, &errs)) {
//		cout << "JSON转换失败.." << errs << endl;
//	}
//	return value;
//}


int main() {
	system("chcp 65001");
	ios::sync_with_stdio(false);
	sizeof(Value);
#if file
	ifstream f("test.json");
	ofstream fout("copy.json");
	string s{ istreambuf_iterator<char>(f), istreambuf_iterator<char>() };
	const char* str = s.c_str();
#else
	const char* str = R"(
{
	"null":null,
	"true":true,
	"false":false,
	"int":25,
	"double":1e-3,
	"double2":1.5934684576,
	"string":"\n\t",
	"array":[null , 1 , 1.0,"test",["\u621188"],{"啊这":1}],
	"object":{"key":true}
}
)";
#endif
	auto start = chrono::steady_clock::now();
	{
		Value v;
		v.parse(str);
		cout << v.toStyledString();
	}


	chrono::duration<double> durString = chrono::steady_clock::now() - start;
	cout << durString.count() << " seconds" << endl;
#if file
	f.close();
	fout.close();
#endif
	return 0;
}

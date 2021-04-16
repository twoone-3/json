#pragma execution_character_set("utf-8")
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#define file 1
using namespace std;
//#include "json/json.h"
//static Json::Value toJson(const string& s) {
//	Json::Value value;
//	Json::Reader reader;
//	if (!reader.parse(s.c_str(), s.c_str() + s.length(), value)) {
//		cerr << reader.getFormattedErrorMessages() << endl;;
//	}
//	return std::move(value);
//}
#include "Json.h"
using namespace Json;
int main() {
	system("chcp 65001");
	ios::sync_with_stdio(false);
	sizeof(Value);
#if file
	ifstream f("test.json");
	ofstream fout("../copy.json");
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
	"string":"Json",
	"array":[null , 1 , 1.0,"test",["\u621188"],{"АЁет":1}],
	"object":{"key":true}
}
)";
#endif
	auto start = chrono::steady_clock::now();//
	Value&& v = toJson(str);
	v.insert("АЁет", 0);
	fout << v << endl;

	chrono::duration<double> durString = chrono::steady_clock::now() - start;
	cout << durString.count() << " seconds" << endl;
#if file
	f.close();
	fout.close();
#endif
	system("pause");
	return 0;
}

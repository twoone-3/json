#pragma execution_character_set("utf-8")
#include <iostream>
#include <fstream>
#include <chrono>
#define file 0
#include "Json.h"
#include "Json.h"
using namespace std;
using namespace Json;

int main() {
	system("chcp 65001");
	ios::sync_with_stdio(false);
	//cout << "sizeof(Json) = " << sizeof(Json) << endl;
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
	"array":[null , 1 , 1.0,"test",["\u6211"],{"АЁет":1}],
	"object":{"key":true}
}
)";
#endif
	auto start = chrono::steady_clock::now();

	cout << toJson(str) << endl;

	chrono::duration<double> durString = chrono::steady_clock::now() - start;
	cout << durString.count() << " seconds" << endl;
#if file
	f.close();
	fout.close();
#endif
	system("pause");
	return 0;
}

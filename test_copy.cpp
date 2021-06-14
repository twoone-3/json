#pragma execution_character_set("utf-8")
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <functional>
#define file 1
using namespace std;
#include "json2.h"
//#include "rapidjson/document.h"
using namespace Json;

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

void test(const function<void()>& fn) {
	auto start = chrono::steady_clock::now();
	fn();
	cout << chrono::duration<double>(chrono::steady_clock::now() - start).count() << " seconds" << endl;
}

int main() {
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
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
	"array":[
		null,
		1,
		1.0,
		"test",
		["\uD834\uDD1E"],
		{"\"АЁет":1}
	],
	"object":{
		"key":true
	}
}
)";
#endif
	auto fn = [] {
		for (size_t i = 0; i < 1; i++)
		{
			Value v(kOBJECT);
			v["az"] = true;
			cout << v << endl;
		}

	};
	test(fn);
	//cout << v << endl;
#if file
	f.close();
	fout.close();
#endif
	//system("pause");
	return 0;
}

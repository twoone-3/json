#pragma execution_character_set("utf-8")
#include <filesystem>
#include <fstream>
#include <Windows.h>
#include "json.h"

using namespace std;

int main() {
	using namespace json;
	using namespace filesystem;
	system("chcp 65001");
	UINT64 start = GetTickCount64();
	for (auto& x : directory_iterator("test")) {
		cout << x << endl;
		ifstream f(x);
		string s(ifstream::_Iter(f), {});
		Value v;
		v.parse(s);
		cout << v << endl;
	}
	UINT64 end = GetTickCount64();

	cout << "程序运行了：" << end - start << "ms" << endl;
	system("pause");
	return 0;
}

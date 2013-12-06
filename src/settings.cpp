#include "settings.h"

#include <fstream>
#include <sstream>

void Settings::load(const string fileName) {
	const char* file = fileName.c_str();

	ifstream in;
	in.open(file);

	while(!in.eof()) {
		string str;
		getline(in, str);
		stringstream ss;
		ss.str(str);
		
		string key, value;
		ss>> key >> value;
		Settings::settings[key] = value;
	}

	in.close();
}

string Settings::get(const string name) {
	return Settings::settings[name];
}

map<string, string> Settings::settings;

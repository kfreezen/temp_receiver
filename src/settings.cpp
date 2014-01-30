#include "settings.h"

#include <fstream>
#include <sstream>

void Settings::store() {
	const char* file = Settings::loadedFileName.c_str();

	ofstream out;
	out.open(file, ofstream::out | ofstream::trunc);

	map<string, string>::iterator itr;
	for(itr = Settings::settings.begin(); itr != Settings::settings.end(); itr++) {
		out<< itr->first << " " << itr->second << "\n";
	}

	out.close();
}

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
	Settings::loadedFileName = fileName;
}

void Settings::set(const string key, const string val) {
	Settings::settings[key] = val;
}

string Settings::get(const string name) {
	return Settings::settings[name];
}

map<string, string> Settings::settings;
string Settings::loadedFileName;
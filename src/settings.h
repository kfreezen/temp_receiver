#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <map>

using namespace std;

class Settings {
public:
	static void load(const string fileName);
	static string get(const string s);
private:
	static map<string, string> settings;
};

#endif
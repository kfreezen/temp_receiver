#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <map>

using namespace std;

class Settings {
public:
	static void load(const string fileName);
	static void store();

	static string get(const string s);
	static void set(const string key, const string value);
private:
	static map<string, string> settings;
	static string loadedFileName;
};

#endif
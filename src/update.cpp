#include "update.h"
#include "curl.h"
#include "settings.h"

#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

extern string receiverId;

int doUpdateCheckerQuit = 0;
void* updateChecker(void* arg) {
	while(!doUpdateCheckerQuit) {
		SimpleCurl curl;
		CURLBuffer* buf = curl.get(Settings::get("server") + "/receiver/current_version", string(""));
		if(buf == NULL) {
			sleep(5);
			continue;
		}

		stringstream ss;
		ss.str(string(buf->buffer));
		int myVersion = CURRENT_VERSION, newVersion;
		ss>> newVersion;
		
		printf("check0\n");
		if(newVersion > myVersion) {
			stringstream url;
			url.str("");
			url << Settings::get("server") << "/receiver/receiver-" << newVersion << ".tar.gz";
			printf("url=%s\n", url.str().c_str());

			// Get our archive.
			curl.download(url.str(), string("receiver-latest.tar.gz"));

			// Execute the update script.
			system("sh update.sh &");
			
		}

		delete buf;

		// Now, check the valid server
		buf = curl.get(Settings::get("server") + "/receiver/valid_server", "");
		if(buf == NULL) {
			sleep(5);
			continue;
		}

		if(string(buf->buffer) != Settings::get("server")) {
			// Test our new server for validity.
			CURLBuffer* result = curl.get(string(buf->buffer) + "/receiver/valid_server", "");
			if(result == NULL) {
				sleep(5);
				continue;
			}

			if(string(result->buffer) == string(buf->buffer)) {
				// Matched
				Settings::set("server", string(buf->buffer));
				Settings::store();
			}
			
			delete result;	
		}

		delete buf;
		
		int sleepLeft = 1800;
		while(sleepLeft) {
			sleepLeft = 300 - sleep(300); // 30 minutes.
			// Check for command.
			stringstream urlstr;
			urlstr.str("");
			urlstr << "/api/network/" << receiverId << "/admin/command";
			CURLBuffer* buf = curl.get(urlstr.str(), "");
			
			if(buf == NULL) {
				continue;
			}

			stringstream bufss;
			bufss.str(string(buf->buffer));
			
			string cmdString, dataString;
			getline(bufss, cmdString, ':');
			getline(bufss, dataString);
			if(cmdString == "reboot") {
				system("reboot");
			} else if(cmdString == "test") {
				FILE* _test = fopen("testCmdSuccessful", "a+");
				fprintf(_test, "Test cmd successfully received.\n");
				fclose(_test);
			}
			
			delete buf;
		}
	}
}

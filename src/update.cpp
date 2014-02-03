#include "update.h"
#include "curl.h"
#include "settings.h"

#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

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
			
			delete archive;
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
			sleepLeft = sleep(sleepLeft); // 30 minutes.
		}
	}
}

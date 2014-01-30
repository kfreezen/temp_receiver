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
		// Now we need to check our version file.
		ifstream versionFile;
		versionFile.open("version");

		stringstream ss;
		ss.str(string(buf->buffer));
		int myVersion, newVersion;
		ss>> newVersion;
		versionFile>> myVersion;
		if(newVersion > myVersion) {
			stringstream url;
			url.str("");
			url << Settings::get("server") << "/receiver/receiver-" << newVersion << ".tar.gz";
			// Get our archive.
			CURLBuffer* archive = curl.get(url.str(), string(""));
			
			// Open "receiver-update.tar.gz" and write the data to it.
			FILE* updateFile = fopen("receiver-update.tar.gz", "w");
			fwrite(archive->buffer, sizeof(char), archive->length, updateFile);
			fclose(updateFile);

			// Execute the update script.
			system("sh update.sh &");
			
			delete archive;
		}

		delete buf;

		// Now, check the valid server
		buf = curl.get(Settings::get("server") + "/receiver/valid_server", "");
		if(string(buf->buffer) != Settings::get("server")) {
			// Test our new server for validity.
			CURLBuffer* result = curl.get(string(buf->buffer) + "/receiver/valid_server", "");
			if(string(result->buffer) == string(buf->buffer)) {
				// Matched
				Settings::set("server", string(buf->buffer));
				Settings::store();
			}
			
			delete result;	
		}

		delete buf;

		sleep(30*60); // 30 minutes.
	}
}
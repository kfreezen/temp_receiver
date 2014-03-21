#ifndef SERVERCOMM_H
#define SERVERCOMM_H

#include <pthread.h>
#include <vector>
#include <map>
#include <string>

#include <cstring>
#include <cctype>
#include <ctime>

using std::map;
using std::string;
using std::vector;

// I thinks the best way to implement this is for it to have its own thread.

class ServerComm {
public:
	ServerComm();
	~ServerComm();

	void start();

	static void* CommThread(void* arg);
	static void* HeartbeatThread(void* arg);

private:
	const static int SERVER_PORT = 14440;
	const static int HEARTBEAT_SECONDS = 10;

	time_t lastHeartbeat;
	int socketFd;

	pthread_t commThread;
	pthread_t heartbeatThread;

	pthread_mutex_t socketMutex;
};

typedef struct {
	char** strings;
	int num;
} StringArray;

class Line {
public:
	Line() {
		this->initValues();
	}

	Line(const char* id, const char* data) {
		this->initValues();

		this->setId(id);
		this->setData(data);
	}

	~Line() {
		if(this->id != NULL) {
			delete[] this->id;
			this->id = NULL;
		}

		if(this->data != NULL) {
			delete[] this->data;
			this->data = NULL;
		}
	}

	const char* getId() {
		return this->id;
	}

	void setId(const char* id) {
		if(id == NULL) {
			return;
		}

		if(this->id != NULL) {
			delete[] this->id;
			this->id = NULL;
		}

		this->id = new char [strlen(id) + 1];
		strcpy(this->id, id);
	}
	
	const char* getData() {
		return this->data;
	}

	void setData(const char* data) {
		if(data == NULL) {
			return;
		}

		if(this->data != NULL) {
			delete[] this->data;
			this->data = NULL;
		}

		this->data = new char[strlen(data) + 1];
		strcpy(this->data, data);
	}

	char* getLineAsString() {
		if(this->id == NULL || this->data == NULL) {
			return NULL;
		}

		// <id>:/s*<data> is the line layout.
		int maxLength = strlen(this->id) + strlen(this->data) + 32;
		char* str = new char[maxLength];
		strncpy(str, this->id, maxLength); // <id>
		strcat(str, ": "); // :/s*
		strcat(str, this->data); // <data>
		return str;
	}

	void setLineFromString(const char* protected_str) {
		char* str = new char[strlen(protected_str) + 1];
		strcpy(str, protected_str);

		// First we fetch the ID.
		int i;
		int strlength = strlen(str);
		for(i = 0; i < strlength; i++) {
			if(str[i] == ':') {
				str[i] = '\0';
				break;
			}
		}

		i++;
		this->setId(str);

		// Zoom through any whitespace
		for(; i < strlength; i++) {
			if(!isspace(str[i])) {
				break;
			}
		}

		// Everything up to the '\n' or '\0' is data.
		char* dataStart = &str[i];
		for(; i < strlength; i++) {
			if(str[i] == '\n' || str[i] == '\0') {
				str[i] = 0;
				break;
			}
		}

		this->setData(dataStart);
		delete[] str;
	}

private:
	void initValues() {
		this->id = NULL;
		this->data = NULL;
	}

	char* id;
	char* data;
};

// Requests
#define CONNECT "CONNECT"
#define HEARTBEAT "HEARTBEAT"

class RSCPContent {
public:
	RSCPContent() {
		this->assembledContent = NULL;
		this->request = NULL;
		this->assembledContentOutOfDate = true;
	}

	~RSCPContent() {
		this->clear();
	}

	void clear() {
		if(this->assembledContent != NULL) {
			delete[] this->assembledContent;
			this->assembledContent = NULL;
		}

		if(this->request != NULL) {
			delete[] this->request;
			this->request = NULL;
		}

		map<string, char*>::iterator itr;
		for(itr = this->contentLines.begin(); itr != this->contentLines.end(); itr++) {
			// Delete our lines one by one. 
			delete[] itr->second;
		}

		this->contentLines.clear();
	}

	const char* getRequest();
	void setRequest(const char* request);

	void addLine(const char* lineId, const char* lineData);
	void addLineString(const char* lineStr);

	const char* getLineData(string key) {
		return this->contentLines[key];
	}

	char* getContentAsString();

	void loadFromString(const char* str);
	void loadFromSocket(int socketFd);

	int sendTo(int socketFd);
	
private:
	bool assembledContentOutOfDate;
	char* assembledContent;

	char* request;

	map<string, char*> contentLines;
};

void ServerCommTests();

#endif
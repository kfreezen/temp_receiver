CPP_SRC:=$(shell find src -mindepth 1 -maxdepth 3 -name "*.cpp")
HDR:=$(shell find src -mindepth 1 -maxdepth 3 -name "*.h")
SOURCES:=$(patsubst %.cpp, %.o, $(CPP_SRC))
CXX:=g++
LDFLAGS:=-lcurl -pthread
LD:=g++

all: $(SOURCES) $(HDR) link

link:
	$(LD) $(SOURCES) $(LDFLAGS) -o receiver

clean:
	-@rm $(SOURCES) receiver
	

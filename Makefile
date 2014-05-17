CPP_SRC:=$(shell find src -mindepth 1 -maxdepth 3 -name "*.cpp")
C_SRC:=$(shell find src -mindepth 1 -maxdepth 3 -name "*.c")

HDR:=$(shell find src -mindepth 1 -maxdepth 3 -name "*.h")

CPP_SOURCES:=$(patsubst %.cpp, %.o, $(CPP_SRC))
C_SOURCES:=$(patsubst %.c, %.o, $(C_SRC))
SOURCES := $(CPP_SOURCES) $(C_SOURCES)
CXX:=g++
CC:=gcc
CXXFLAGS:=-std=gnu++11 -g
LDFLAGS:=`curl-config --libs` -lm -pthread
LD:=g++

all: $(SOURCES) $(HDR) link

link:
	$(LD) $(SOURCES) $(LDFLAGS) -o receiver

clean:
	-@rm $(SOURCES) receiver

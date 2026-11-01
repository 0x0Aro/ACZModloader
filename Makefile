CXX := x86_64-w64-mingw32-g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?= -municode -mwindows -static-libgcc -static-libstdc++ -s
LIBS ?= -lshell32 -luser32

BUILD_DIR := build
TARGET := $(BUILD_DIR)/ACZModLoader.exe
SOURCES := src/mod_loader.cpp
HEADERS := src/mod_common.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SOURCES) $(LIBS)

clean:
	rm -rf $(BUILD_DIR)

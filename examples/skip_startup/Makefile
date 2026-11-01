CXX := x86_64-w64-mingw32-g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
DLLFLAGS ?= -shared -static-libgcc -static-libstdc++ -s
LIBS ?= -luser32 -lkernel32

BUILD_DIR := ../../build
TARGET := $(BUILD_DIR)/skip_startup.dll
SOURCES := skip_startup.cpp
HEADERS := mod_common.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(DLLFLAGS) -o $@ $(SOURCES) $(LIBS)

clean:
	rm -f $(TARGET)

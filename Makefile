.PHONY: all clean

all: wfq.exe

wfq.exe: wfq.cpp
	clang wfq.cpp --std=c++20 -Wall -Wextra -Wpedantic -o wfq.exe

clean:
	del wfq.exe

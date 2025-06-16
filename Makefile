.PHONY: all clean

all: wfq.exe new_wfq.exe

wfq.exe: wfq.cpp
	clang wfq.cpp --std=c++20 -Wall -Wextra -Wpedantic -o wfq.exe

new_wfq.exe: new_wfq.cpp
	clang new_wfq.cpp --std=c++20 -Wall -Wextra -Wpedantic -o new_wfq.exe

clean:
	del wfq.exe
	del new_wfq.exe

.PHONY: all clean

all: wfq.exe

wfq.exe: wfq.cpp
	clang wfq.cpp -o wfq.exe

clean:
	del wfq.exe

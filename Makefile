# compile with symbols for backtrace
CXXFLAGS=-g
# add symbols to dynamic symbol table for backtrace
LDFLAGS=-rdynamic

all: example hack

example: get_stacktrace_on_exception.cpp
	g++ $(CXXFLAGS) $(LDFLAGS) -std=c++11 -O2 -ldl -fdiagnostics-color=always $^ -o $@

hack: stacktrace_test.cpp stacktrace_on_exception.cpp stacktrace_on_exception.h
	g++ $(CXXFLAGS) $(LDFLAGS) -std=c++11 -O2 -ldl -fdiagnostics-color=always $^ -o $@

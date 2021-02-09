# compile with symbols for backtrace
CXXFLAGS=-g
# add symbols to dynamic symbol table for backtrace
LDFLAGS=-rdynamic

all: example hack

example: get_stacktrace_on_exception.cpp
	g++ $(CXXFLAGS) $(LDFLAGS) -std=c++11 -O2 -ldl -fdiagnostics-color=always $^ -o $@

hack: stacktrace_test.cpp stacktrace_on_exception.cpp stacktrace_on_exception.h
	g++ $(CXXFLAGS) $(LDFLAGS) -std=c++11 -O2 -ldl -fdiagnostics-color=always $^ -o $@

stacktrace_on_exception.o: stacktrace_on_exception.cpp stacktrace_on_exception.h
	g++ -c -fPIC $(CXXFLAGS) -std=c++11 -O2 $(LDFLAGS) -ldl -fdiagnostics-color=always $< -o $@

libstacktrace_on_exception.so: stacktrace_on_exception.o
	g++ $(CXXFLAGS) $(LDFLAGS) -fPIC -shared -std=c++11 -O2 -ldl -fdiagnostics-color=always $^ -o $@

hack2: stacktrace_test.cpp libstacktrace_on_exception.so
	LD_RUN_PATH=${PWD} g++ $(CXXFLAGS) -std=c++11 -O2 $(LDFLAGS) $< -L. -lstacktrace_on_exception -o $@

# don't even link against it, just load the library when calling the executable with:
# `LD_PRELOAD=/path/to/libstacktrace_on_exception.so ./hack3`
hack3: stacktrace_test.cpp
	g++ $(CXXFLAGS) -std=c++11 -O2 $^ -o $@

clean:
	@rm -f example
	@rm -f hack
	@rm -f hack2
	@rm -f stacktrace_on_exception.o
	@rm -f libstacktrace_on_exception.so

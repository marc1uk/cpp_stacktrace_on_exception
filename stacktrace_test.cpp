#include <vector>
#include <map>

int main(int, char**)
{
    try {
        try {
            std::printf("Test 1: throwing runtime_error '123'\n");
            throw std::runtime_error("123");
            std::vector<int> someints{1,2,3};
            int ret = someints.at(4);
        }
        catch (const std::exception& e) {
            std::printf("caught %s\n\n", e.what());
            std::printf("Test 2: re-throwing this exception (runtime_error '123')\n");
            throw;
        }
    }
    catch (const std::exception& d) {
        std::printf("caught rethrown %s\n", d.what());
    }

    try {
        std::printf("Test 3: throwing int '1'\n");
        throw 1;
        std::map<int,int> mm;
        mm.at(1);
    }
    catch (int x) {
        std::printf("caught %d\n", x);
    }
   
    return 0;
}

/* NOTES:
Note there's nothing here that even references stacktrace_on_exception.cpp or stacktrace_on_exception.h
I guess just by including them in the makefile, it shadows the libcstd functions, and as they're
included by default, you don't need to reference them here. ¯\_(ツ)_/¯


something weird and finnicky is going on here. If this is built with compiler optimization -O2,
everything works as expected. If this is built with compiler optimization -O0, and the print statement
`std::printf("Test 3: throwing int '1'\n");` (line 28) is commented out, everything works as expected.
But, if line 28 is present and -O0 is used, then the stack traces printed point to lines 29, 20, then 29.
It should be 13, 20, 29. i.e., it reportedly throws exception 3, then 2 then 3 again, instead of 1, 2, 3.
Moving the print statement one line up, outside the try, makes it work again even with -O0.
What's going on!!? Who knows. Ummm... maybe just, always use compiler optimization -O2???
*/


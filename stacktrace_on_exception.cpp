#include "stacktrace_on_exception.h"

namespace { // Important: without an anonymous namespace, gcc starts to go crazy

// Get call trace
// Hereinafter, functions from the C standard library are deliberately used, since they do not throw exceptions
// An exception in a low-level exception handler is something
static void get_backtrace(void)
{
    static void* buf[128];

    int n = backtrace(buf, 128);
    /*
    // Uncomment these two lines to print the raw backtrace
    std::fprintf(stderr, "%s\n", "*** BACKTRACE ***");
    backtrace_symbols_fd(buf, n, STDERR_FILENO);
    */

    // Code below assumes readlink and addr2line programs are installed in /usr/bin/
    // the idea is to feed the addresses received by backtrace (3) to addr2line (1)
    // in order to get a human-readable result
    std::size_t bufsize = 19*n + std::strlen("/usr/bin/addr2line -pifCa -e `/bin/readlink /proc/XXXXX/exe` 1>&2") + 1;
    char* space = reinterpret_cast<char*>(std::calloc(bufsize, 1));
    if (space) {
        char* orig = space;
        int c = std::sprintf(space, "/usr/bin/addr2line -pifCa -e `/bin/readlink /proc/%d/exe` ", getpid());
        space += c;
        for (int i=0; i<n; ++i) {  // skip lines 0-2 (backtrace of this file, stacktrace_on_exception.cpp)
            c = std::sprintf(space, "%p ", buf[i]);
            space += c;
        }
        std::sprintf(space, "%s", "1>&2");

        std::fprintf(stderr, "%s\n", "\n*** DECODED BACKTRACE ***");

        //std::fprintf(stderr, "%s\n", orig);   // prints the command about to be executed
        if (std::system(orig)) {} // So that gcc doesn't swear
        std::free(orig);
    }
}

// Exception handling common to __cxz_throw and __cxa_rethrow
static void handle_exception(void* thrown_exception, std::type_info* tinfo, bool rethrown)
{
    char* demangled = abi:: __cxa_demangle(tinfo->name(), 0, 0, 0);
    std::fprintf(stderr, "%s exception of type %s\n", (rethrown ? "Rethrown" : "Thrown"), (demangled ? demangled : tinfo->name()));
    if (demangled) {
        std::free(demangled);
    }

    const abi::__class_type_info* exc = dynamic_cast<const abi::__class_type_info*>(&typeid(std::exception));
    const abi::__class_type_info* cti = dynamic_cast<abi::__class_type_info*>(tinfo);

    if (cti && exc) {
        std::exception* the_exception = reinterpret_cast<std::exception*>(abi::__dynamic_cast(thrown_exception, exc, cti, -1));
        if (the_exception) {
            std::fprintf(stderr, "what(): %s\n", the_exception->what());
        }
    }

    std::fprintf(stderr, "▿▿▿▿▿▿▿▿▿▿▿");
    get_backtrace();
    std::fprintf(stderr, "▵▵▵▵▵▵▵▵▵▵▵\n\n");
}

// Initialize variables. You can do this in the handler, but it's safer this way
static void initialize(void)
{
    orig_cxa_throw   = reinterpret_cast<cxa_throw_type>(dlsym(RTLD_NEXT, "__cxa_throw"));
    orig_cxa_rethrow = reinterpret_cast<cxa_rethrow_type>(dlsym(RTLD_NEXT, "__cxa_rethrow"));
}

extern "C" void __cxa_throw(void* thrown_exception, std::type_info* tinfo, void (*dest)(void*))
{
    pthread_mutex_lock(&guard);
    handle_exception(thrown_exception, tinfo, false);
    pthread_mutex_unlock(&guard);
    
    // original code, requires orig_cxa_throw to be set via an initialize call
    // which it says is "safer"
    if(orig_cxa_throw==0) orig_cxa_throw   = reinterpret_cast<cxa_throw_type>(dlsym(RTLD_NEXT, "__cxa_throw"));
    
    if (orig_cxa_throw) {
        orig_cxa_throw(thrown_exception, tinfo, dest);
    }
    else {
        std::terminate();
    }
    
}

extern "C" void __cxa_rethrow(void)
{
    pthread_mutex_lock(&guard);
    __cxa_eh_globals* g = __cxa_get_globals();
    if (g && g->caughtExceptions) {
        void* thrown_exception = reinterpret_cast<uint8_t*>(g->caughtExceptions) + sizeof(struct __cxa_exception);
        handle_exception(thrown_exception, g->caughtExceptions->exceptionType, true);
    }

    pthread_mutex_unlock(&guard);

    if(orig_cxa_rethrow==0) orig_cxa_rethrow = reinterpret_cast<cxa_rethrow_type>(dlsym(RTLD_NEXT, "__cxa_rethrow"));
    if (orig_cxa_rethrow) {
        orig_cxa_rethrow();
    }
    else {
        std::terminate();
    }

}

}  // end anonymous namespace


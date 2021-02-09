// from https://web.archive.org/web/20150706050621/http://blog.sjinks.pro/c-cpp/969-track-uncaught-exceptions
// credit Опубликовано Vladimir (?)
#include <typeinfo>
#include <exception>
#include <dlfcn.h>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <inttypes.h>
#include <execinfo.h>
#include <cxxabi.h> // Defines types from namespace abi
#include <cstring>
#include <stdexcept>
#include <unistd.h>

namespace { // Important: without an anonymous namespace, gcc starts to go crazy

// Data types / structures from http://sourcery.mentor.com/public/cxx-abi/abi-eh.html
typedef enum {
    _URC_NO_REASON = 0,
    _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
    _URC_FATAL_PHASE2_ERROR = 2,
    _URC_FATAL_PHASE1_ERROR = 3,
    _URC_NORMAL_STOP = 4,
    _URC_END_OF_STACK = 5,
    _URC_HANDLER_FOUND = 6,
    _URC_INSTALL_CONTEXT = 7,
    _URC_CONTINUE_UNWIND = 8
} _Unwind_Reason_Code;

typedef void (*_Unwind_Exception_Cleanup_Fn)(_Unwind_Reason_Code reason, struct _Unwind_Exception* exc);

struct _Unwind_Exception {
    uint64_t exception_class;
    _Unwind_Exception_Cleanup_Fn exception_cleanup;
    uint64_t private_1;
    uint64_t private_2;
};

struct __cxa_exception {
    std::type_info* exceptionType;
    void (*exceptionDestructor)(void*);
    std::unexpected_handler unexpectedHandler;
    std::terminate_handler terminateHandler;
    __cxa_exception* nextException;
    int handlerCount;
    int handlerSwitchValue;
    const char* actionRecord;
    const char* languageSpecificData;
    void* catchTemp;
    void* adjustedPtr;
    _Unwind_Exception unwindHeader;
};

struct __cxa_eh_globals {
    __cxa_exception* caughtExceptions;
    unsigned int uncaughtExceptions;
};

extern "C" __cxa_eh_globals* __cxa_get_globals(void);

// Function type __cxa_throw
typedef void(*cxa_throw_type)(void*, std::type_info*, void(*)(void*));
// Function type __cxa_rethrow
typedef void(*cxa_rethrow_type)(void);

static cxa_throw_type   orig_cxa_throw   = 0; // Address of the original function __cxa_throw
static cxa_rethrow_type orig_cxa_rethrow = 0; // Address of the original function __cxa_rethrow

static pthread_mutex_t guard = PTHREAD_MUTEX_INITIALIZER;  // Mutex for every fireman

// Get call trace
// Hereinafter, functions from the C standard library are deliberately used, since they do not throw exceptions
// An exception in a low-level exception handler is something
static void get_backtrace(void)
{
    static void* buf[128];

    int n = backtrace(buf, 128);
    std::fprintf(stderr, "%s\n", "*** BACKTRACE ***");
    backtrace_symbols_fd(buf, n, STDERR_FILENO);

    // Code below assumes readlink and addr2line programs are installed in /usr/bin/
    // the idea is to feed the addresses received by backtrace (3) to addr2line (1)
    // in order to get a human-readable result
    std::size_t bufsize = 19*n + std::strlen("/usr/bin/addr2line -pifCa -e `/bin/readlink /proc/XXXXX/exe` 1>&2") + 1;
    char* space = reinterpret_cast<char*>(std::calloc(bufsize, 1));
    if (space) {
        char* orig = space;
        int c = std::sprintf(space, "/usr/bin/addr2line -pifCa -e `/bin/readlink /proc/%d/exe` ", getpid());
        space += c;
        for (int i=0; i<n; ++i) {
            c = std::sprintf(space, "%p ", buf[i]);
            space += c;
        }

        std::fprintf(stderr, "%s\n", "\n*** DECODED BACKTRACE ***");
        std::sprintf(space, "%s", "1>&2");

        std::fprintf(stderr, "%s\n", orig);
        if (std::system(orig)) {} // Чтобы не ругался gcc
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

    get_backtrace();
    std::fprintf(stderr, "\n\n");
}

extern "C" void __cxa_throw(void* thrown_exception, std::type_info* tinfo, void (*dest)(void*))
{
    pthread_mutex_lock(&guard);
    handle_exception(thrown_exception, tinfo, false);
    pthread_mutex_unlock(&guard);
   
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

    if (orig_cxa_rethrow) {
        orig_cxa_rethrow();
    }
    else {
        std::terminate();
    }
}

// Initialize variables. You can do this in the handler, but it's safer this way
static void initialize(void)
{
    orig_cxa_throw   = reinterpret_cast<cxa_throw_type>(dlsym(RTLD_NEXT, "__cxa_throw"));
    orig_cxa_rethrow = reinterpret_cast<cxa_rethrow_type>(dlsym(RTLD_NEXT, "__cxa_rethrow"));
}

}

int main(int, char**)
{
    initialize();

    try {
        try {
            throw std::runtime_error("123");
        }
        catch (const std::exception& e) {
            std::printf("e.what(): %s\n", e.what());
            throw;
        }
    }
    catch (const std::exception& d) {
        std::printf("d.what(): %s\n", d.what());
    }

    try {
        throw 1;
    }
    catch (int x) {
        std::printf("%d\n", x);
    }
   
    return 0;
}

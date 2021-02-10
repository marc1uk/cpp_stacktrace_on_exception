#include <typeinfo>
#include <exception>
#include <dlfcn.h>
#include <pthread.h>
#include <cstdio>   // deprecated equivalent without std:: namespace, 'stdio.h'
#include <cstdlib>  // deprecated equivalent without std:: namespace, 'stdlib.h'
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
// An exception in a low-level exception handler is something to avoid
static void get_backtrace(void);

// Exception handling common to __cxz_throw and __cxa_rethrow
static void handle_exception(void* thrown_exception, std::type_info* tinfo, bool rethrown);

// Initialize variables. You can do this in the handler, but it's safer this way
static void initialize(void);

extern "C" void __cxa_throw(void* thrown_exception, std::type_info* tinfo, void (*dest)(void*));
extern "C" void __cxa_rethrow(void);

// swap-in alternatives to printing the backtrace
size_t ConvertToVMA(size_t addr);
void get_backtrace2();
void get_backtrace3();
/*void get_backtrace4(); // depends on boost*/

}  // end anonymous namespace


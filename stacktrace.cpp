#include "stacktrace.hpp"
#include "config.hpp"

#ifndef NO_STACKTRACE

//#define BOOST_STACKTRACE_USE_BACKTRACE
#define BOOST_STACKTRACE_USE_WINDBG_CACHED

#include <signal.h>     // ::signal, ::raise
#include <boost/stacktrace.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <fstream>

void signal_handler(int signum)
{
    ::signal(signum, SIG_DFL);
    //boost::stacktrace::safe_dump_to("./backtrace.dump");

    std::string stacktrace = get_stacktrace();

    printf("stacktrace %s\n", stacktrace.c_str());

    FILE* pFile = fopen("crash.txt", "a+");

    fwrite(stacktrace.c_str(), 1, stacktrace.size(), pFile);

    fclose(pFile);

    system("pause");

    ::raise(SIGABRT);
}

void stack_on_start()
{
    //CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    ::signal(SIGSEGV, &signal_handler);
    ::signal(SIGABRT, &signal_handler);

    std::cout << "Initialised stacktrace handling" << std::endl;

    #ifdef __WIN32__
    if (std::filesystem::exists("./backtrace.dump"))
    {
        // there is a backtrace
        std::ifstream ifs("./backtrace.dump");

        boost::stacktrace::stacktrace st = boost::stacktrace::stacktrace::from_dump(ifs);
        std::cout << "Previous run crashed:\n" << st << std::endl;

        // cleaning up
        ifs.close();
        if(std::filesystem::exists("./backtrace_1.dump"))
            std::filesystem::remove("./backtrace_1.dump");

        rename("./backtrace.dump", "./backtrace_1.dump");
    }
    #endif // __WIN32__
}

std::string get_stacktrace()
{
    std::stringstream stream;

    stream << boost::stacktrace::stacktrace();

    return stream.str();
}

stack_frame frame_from_ptr(void* ptr)
{
    stack_frame frame;
    boost::stacktrace::frame boost_frame((boost::stacktrace::detail::native_frame_ptr_t)ptr);

    frame.name = boost_frame.name();
    frame.file = boost_frame.source_file();
    frame.line = boost_frame.source_line();

    return frame;
}

struct static_helper
{
    static_helper()
    {
        stack_on_start();
    }
};

static static_helper help;
#else
std::string get_stacktrace()
{
    return "Stacktracing unimplemented on this platform, emscripten?";
}
#endif

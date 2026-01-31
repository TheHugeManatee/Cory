#include <Cory/Base/Log.hpp>

#include <Cory/Base/Debugger.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace Cory {
void AssertionFailed(std::string_view condition,
                     std::string_view messageWithDetails,
                     std::source_location location)
{
    CO_CORE_FATAL(R"(
**** Assertion Failure *****
    "{}"
{} != true
    at {}:{} in function {}
)",
                  messageWithDetails,
                  condition,
                  location.file_name(),
                  location.line(),
                  location.function_name());
    Log::Shutdown(); // Flushes logs before aborting
    BreakpointIfDebugging();
    std::abort();
}

void Log::Init()
{
    // spdlog::set_pattern("%^[%T] %n: %v%$");

    auto &coreLogger = Log::GetCoreLogger();
    auto &appLogger = Log::GetAppLogger();
    coreLogger = spdlog::stdout_color_mt("Cory");
    appLogger = spdlog::stdout_color_mt("App");

    coreLogger->set_level(spdlog::level::debug);
    appLogger->set_level(spdlog::level::trace);
    auto atexit_success = atexit([]() { Log::Shutdown(); });
    CO_CORE_ASSERT(atexit_success == 0,
                   "Could not register atexit handler for deinitializing logs");
}

void Log::Shutdown()
{
    spdlog::shutdown();
}
} // namespace Cory

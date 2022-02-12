#include "Log.h"

#include <cvk/log.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Cory {
std::shared_ptr<spdlog::logger> Log::s_coreLogger;
std::shared_ptr<spdlog::logger> Log::s_appLogger;

void Log::Init()
{
    cvk::log::init();

    // spdlog::set_pattern("%^[%T] %n: %v%$");

    s_coreLogger = spdlog::stdout_color_mt("Cory");
    s_appLogger = spdlog::stdout_color_mt("App");

    s_coreLogger->set_level(spdlog::level::debug);
    s_appLogger->set_level(spdlog::level::trace);
}
} // namespace Cory

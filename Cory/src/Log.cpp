#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace Cory {
std::shared_ptr<spdlog::logger> Log::s_coreLogger;
std::shared_ptr<spdlog::logger> Log::s_appLogger;

void Log::Init() { 
	
	//spdlog::set_pattern("%^[%T] %n: %v%$"); 

	s_coreLogger = spdlog::stdout_color_mt("Cory");
	s_appLogger = spdlog::stdout_color_mt("App");

}
} // namespace Cory

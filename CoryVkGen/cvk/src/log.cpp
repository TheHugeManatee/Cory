#include <cvk/log.h>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace cvk {
std::shared_ptr<spdlog::logger> log::logger_;

void log::init()
{
    // spdlog::set_pattern("%^[%T] %n: %v%$");

    logger_ = spdlog::stdout_color_mt("cvk");
    logger_->set_level(spdlog::level::debug);
}
} // namespace Cory

#pragma once

#include <spdlog/spdlog.h>

#include <memory>

namespace cvk {
class log {
  public:
    static void init();

    inline static std::shared_ptr<spdlog::logger> &logger() { return logger_; }

    inline static void set_level(spdlog::level::level_enum level) { logger_->set_level(level); }

    inline static auto level() { return logger_->level(); }

    inline static auto set_level_scoped(spdlog::level::level_enum level)
    {
        return scoped_log_level{*logger_, level};
    }

  private:
    struct scoped_log_level {
        scoped_log_level(spdlog::logger &logger, spdlog::level::level_enum level)
            : logger_{logger}
            , prev_level_{logger.level()}
        {
            logger.set_level(level);
        }
        ~scoped_log_level() { logger_.set_level(prev_level_); }
        spdlog::logger &logger_;
        spdlog::level::level_enum prev_level_;
    };

    static std::shared_ptr<spdlog::logger> logger_;
};

} // namespace cvk

#define CVK_FATAL(...) ::cvk::log::logger()->critical(__VA_ARGS__)
#define CVK_ERROR(...) ::cvk::log::logger()->error(__VA_ARGS__)
#define CVK_WARN(...)  ::cvk::log::logger()->warn(__VA_ARGS__)
#define CVK_DEBUG(...) ::cvk::log::logger()->debug(__VA_ARGS__)
#define CVK_TRACE(...) ::cvk::log::logger()->trace(__VA_ARGS__)
#define CVK_INFO(...)  ::cvk::log::logger()->info(__VA_ARGS__)

#define CVK_ASSERT(condition, message, ...)                                                        \
    if (auto val = (condition); !(val)) {                                                          \
        auto formatted_message = fmt::format(message, __VA_ARGS__);                                \
        auto assertion_string =                                                                    \
            fmt::format("Assertion failed: {}\n{} == {}.\n", formatted_message, #condition, val);  \
        CO_CORE_FATAL(assertion_string);                                                           \
        std::abort();                                                                              \
    }
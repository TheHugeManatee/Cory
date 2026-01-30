#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <source_location>

namespace Cory {
class Log {
  public:
    static void Init();
    static void Shutdown();

    static std::shared_ptr<spdlog::logger> &GetCoreLogger() { return s_coreLogger; }
    static std::shared_ptr<spdlog::logger> &GetAppLogger() { return s_appLogger; }

    static void SetCoreLevel(spdlog::level::level_enum level) { s_coreLogger->set_level(level); }
    static void SetAppLevel(spdlog::level::level_enum level) { s_appLogger->set_level(level); }
    static auto GetCoreLevel() { return s_coreLogger->level(); }
    static auto GetAppLevel() { return s_appLogger->level(); }

    static auto SetCoreLevelScoped(spdlog::level::level_enum level)
    {
        return ScopedLogLevel{*s_coreLogger, level};
    }
    static auto SetAppLevelScoped(spdlog::level::level_enum level)
    {
        return ScopedLogLevel{*s_appLogger, level};
    }

    struct MemorySize {
        size_t bytes;
    };
    [[nodiscard]] static MemorySize asMemorySize(size_t bytes) { return MemorySize{bytes}; }

  private:
    class ScopedLogLevel {
      public:
        ScopedLogLevel(spdlog::logger &logger, spdlog::level::level_enum level)
            : logger_{logger}
            , prev_level_{logger.level()}
        {
            logger.set_level(level);
        }
        ~ScopedLogLevel() { logger_.set_level(prev_level_); }

      private:
        spdlog::logger &logger_;
        spdlog::level::level_enum prev_level_;
    };

    static std::shared_ptr<spdlog::logger> s_coreLogger;
    static std::shared_ptr<spdlog::logger> s_appLogger;
};

/// Log assertion failure and abort in a controlled manner. Intended usage via CO_CORE_ASSERT.
[[noreturn]] void AssertionFailed(std::string_view condition,
                                  std::string_view messageWithDetails,
                                  std::source_location location = std::source_location::current());

} // namespace Cory

#define CO_CORE_FATAL(...) ::Cory::Log::GetCoreLogger()->critical(__VA_ARGS__)
#define CO_CORE_ERROR(...) ::Cory::Log::GetCoreLogger()->error(__VA_ARGS__)
#define CO_CORE_WARN(...)  ::Cory::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define CO_CORE_DEBUG(...) ::Cory::Log::GetCoreLogger()->debug(__VA_ARGS__)
#ifdef _DEBUG
#define CO_CORE_TRACE(...) ::Cory::Log::GetCoreLogger()->trace(__VA_ARGS__)
#else
#define CO_CORE_TRACE(...)
#endif
#define CO_CORE_INFO(...) ::Cory::Log::GetCoreLogger()->info(__VA_ARGS__)

#define CO_APP_FATAL(...) ::Cory::Log::GetAppLogger()->critical(__VA_ARGS__)
#define CO_APP_ERROR(...) ::Cory::Log::GetAppLogger()->error(__VA_ARGS__)
#define CO_APP_WARN(...)  ::Cory::Log::GetAppLogger()->warn(__VA_ARGS__)
#define CO_APP_DEBUG(...) ::Cory::Log::GetAppLogger()->debug(__VA_ARGS__)
#ifdef _DEBUG
#define CO_APP_TRACE(...) ::Cory::Log::GetAppLogger()->trace(__VA_ARGS__)
#else
#define CO_APP_TRACE(...)
#endif
#define CO_APP_INFO(...) ::Cory::Log::GetAppLogger()->info(__VA_ARGS__)

#define CO_CORE_ASSERT(condition, message, ...)                                                    \
    if (!(condition)) {                                                                            \
        const auto formattedMessage = fmt::format(message __VA_OPT__(, ) __VA_ARGS__);             \
        Cory::AssertionFailed(#condition, formattedMessage);                                       \
    }

#ifdef _DEBUG
#define CO_CORE_DEBUG_ASSERT(condition, message, ...)                                              \
    CO_CORE_ASSERT(condition, message, __VA_ARGS__)
#else
#define CO_CORE_DEBUG_ASSERT(condition, message, ...)
#endif

template <> struct fmt::formatter<Cory::Log::MemorySize> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const Cory::Log::MemorySize &size, FormatContext &ctx) const
    {
        constexpr size_t KB = 1024;
        constexpr size_t MB = 1024 * KB;
        constexpr size_t GB = 1024 * MB;

        if (size.bytes >= GB)
            return fmt::format_to(ctx.out(), "{:.2f} GB", static_cast<double>(size.bytes) / GB);
        if (size.bytes >= MB)
            return fmt::format_to(ctx.out(), "{:.2f} MB", static_cast<double>(size.bytes) / MB);
        if (size.bytes >= KB)
            return fmt::format_to(ctx.out(), "{:.2f} KB", static_cast<double>(size.bytes) / KB);

        return fmt::format_to(ctx.out(), "{} B", size.bytes);
    }
};

#pragma once

#include <spdlog/spdlog.h>

#include <memory>

namespace Cory {
class Log {
public:
  static void Init();

  inline static std::shared_ptr<spdlog::logger> &GetCoreLogger()
  {
    return s_coreLogger;
  }
  inline static std::shared_ptr<spdlog::logger> &GetAppLogger()
  {
    return s_appLogger;
  }

  inline static void SetCoreLevel(spdlog::level::level_enum level)
  {
    s_coreLogger->set_level(level);
  }
  inline static void SetAppLevel(spdlog::level::level_enum level)
  {
    s_coreLogger->set_level(level);
  }

private:
  static std::shared_ptr<spdlog::logger> s_coreLogger;
  static std::shared_ptr<spdlog::logger> s_appLogger;
};

} // namespace Cory


#define CO_CORE_FATAL(...) ::Cory::Log::GetCoreLogger()->critical(__VA_ARGS__)
#define CO_CORE_ERROR(...) ::Cory::Log::GetCoreLogger()->error(__VA_ARGS__)
#define CO_CORE_WARN(...) ::Cory::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define CO_CORE_DEBUG(...) ::Cory::Log::GetCoreLogger()->debug(__VA_ARGS__)
#define CO_CORE_TRACE(...) ::Cory::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define CO_CORE_INFO(...) ::Cory::Log::GetCoreLogger()->info(__VA_ARGS__)

#define CO_APP_FATAL(...) ::Cory::Log::GetAppLogger()->critical(__VA_ARGS__)
#define CO_APP_ERROR(...) ::Cory::Log::GetAppLogger()->error(__VA_ARGS__)
#define CO_APP_WARN(...) ::Cory::Log::GetAppLogger()->warn(__VA_ARGS__)
#define CO_APP_DEBUG(...) ::Cory::Log::GetAppLogger()->debug(__VA_ARGS__)
#define CO_APP_TRACE(...) ::Cory::Log::GetAppLogger()->trace(__VA_ARGS__)
#define CO_APP_INFO(...) ::Cory::Log::GetAppLogger()->info(__VA_ARGS__)

#define CO_CORE_ASSERT(condition, message)                                                              \
    if (!(condition)) { CO_CORE_FATAL("Assertion failed: \n{}.\n    {}", #condition, message);}
#include "Debugger.hpp"

#include <csignal>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(__linux__)
#include <cctype>
#endif

namespace Cory {
void Breakpoint() noexcept
{
#if defined(_MSC_VER)
    __debugbreak();
#elif defined(__has_builtin)
#if __has_builtin(__builtin_debugtrap)
    __builtin_debugtrap();
#elif __has_builtin(__builtin_trap)
    __builtin_trap(); // Not ideal, but last resort
#else
    raise(SIGTRAP);
#endif
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_trap(); // May SIGILL on some archs
#else
    raise(SIGTRAP);
#endif
}

[[nodiscard]] bool DebuggerAttached() noexcept
{
#if defined(_WIN32)
    return ::IsDebuggerPresent();
#elif defined(__APPLE__)
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    struct kinfo_proc info{};
    size_t size = sizeof(info);

    if (sysctl(mib, 4, &info, &size, nullptr, 0) == 0) {
        return (info.kp_proc.p_flag & P_TRACED) != 0;
    }
    return false;
#elif defined(__linux__)
    std::ifstream status("/proc/self/status");
    if (!status) return false;

    std::string line;
    constexpr std::string_view tracer("TracerPid:");
    while (std::getline(status, line)) {
        if (line.rfind(tracer, 0) == 0) {
            const auto value = line.substr(tracer.size());
            auto idx = value.find_first_not_of(" \t");
            if (idx != std::string::npos) {
                const auto remainder = value.substr(idx);
                const auto pid = std::stoi(remainder);
                return pid != 0;
            }
            break;
        }
    }
    return false;
#else
    return false;
#endif
}

} // namespace Cory

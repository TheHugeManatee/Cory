#pragma once

namespace Cory {

/// Detect debugger being attached
[[nodiscard]] bool DebuggerAttached() noexcept;

/// Trigger a breakpoint unconditionally
void Breakpoint() noexcept;

/// Trigger a breakpoint only if a debugger is attached
inline void BreakpointIfDebugging() noexcept
{
    if (DebuggerAttached()) {
        Breakpoint();
    }
}

} // namespace Cory

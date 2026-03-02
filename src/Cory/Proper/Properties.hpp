#pragma once

#include <Cory/Base/Debugger.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/SlotMapHandle.hpp>

#include <cppcoro/coroutine.hpp>
#include <glm/vec3.hpp>

#include <concepts>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

namespace Cory::Proper {

class PropertySet;

struct Int {
    int value{0};
    int min{std::numeric_limits<int>::min()};
    int max{std::numeric_limits<int>::max()};
    int step{1};
};

struct Float {
    float value{0.0f};
    float min{-std::numeric_limits<float>::infinity()};
    float max{std::numeric_limits<float>::infinity()};
    float step{0.01f};
};

struct Vec3 {
    glm::vec3 value{0.0f};
    glm::vec3 min{-std::numeric_limits<float>::infinity()};
    glm::vec3 max{std::numeric_limits<float>::infinity()};
    glm::vec3 step{0.01f};
};

struct String {
    std::string value{};
};

using PropertyVariant = std::variant<Int, Float, Vec3, String>;
using PropertyValue = std::variant<int, float, glm::vec3, std::string>;
using PropertyHandle = PrivateTypedHandle<PropertyVariant, PropertySet>;
struct Group;
using GroupHandle = PrivateTypedHandle<Group, PropertySet>;

struct Group {
    std::unordered_map<std::string, std::variant<PropertyHandle, GroupHandle>> children;
};

template <typename T>
concept IsProperty = std::same_as<T, Int> || std::same_as<T, Float> || std::same_as<T, Vec3> ||
                     std::same_as<T, String>;

template <typename T>
concept IsPropertyValue = std::same_as<T, int> || std::same_as<T, float> ||
                          std::same_as<T, glm::vec3> || std::same_as<T, std::string>;

template <typename T>
concept IsRangeProperty = std::same_as<T, Int> || std::same_as<T, Float> || std::same_as<T, Vec3>;

class AbstractProperty {
  public:
    virtual ~AbstractProperty() = default;

    // cancellation support for waiting on property changes
    void registerWaiter(cppcoro::coroutine_handle<> waiter);
    void unregisterWaiter(cppcoro::coroutine_handle<> waiter);

    void notifyWaiters();

  private:
    std::vector<cppcoro::coroutine_handle<>> waiters_;
};

template <typename T = void> class Task;

template <> class Task<void> {
  public:
    struct promise_type {
        Task get_return_object() noexcept
        {
            return Task{cppcoro::coroutine_handle<promise_type>::from_promise(*this)};
        }
        cppcoro::suspend_never initial_suspend() noexcept { return {}; }
        cppcoro::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        [[noreturn]] void unhandled_exception()
        {
            BreakpointIfDebugging();
            std::terminate();
        }

        void setCancellation(AbstractProperty *property, cppcoro::coroutine_handle<> awaiting)
        {
            cancelProperty_ = property;
            cancelAwaiting_ = awaiting;
        }
        void clearCancellation()
        {
            cancelProperty_ = {};
            cancelAwaiting_ = {};
        }
        void cancel();

      private:
        AbstractProperty *cancelProperty_{nullptr};
        cppcoro::coroutine_handle<> cancelAwaiting_{};
    };

    using Handle = cppcoro::coroutine_handle<promise_type>;

    Task() = default;
    explicit Task(Handle handle) noexcept
        : handle_{handle}
    {
    }
    Task(Task &&other) noexcept
        : handle_{std::exchange(other.handle_, {})}
    {
    }
    Task &operator=(Task &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        cancel();
        handle_ = std::exchange(other.handle_, {});
        return *this;
    }
    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;

    /// Cancel the task if it is still active
    void cancel()
    {
        if (!handle_) {
            return;
        }

        handle_.promise().cancel();
        handle_.destroy();
        handle_ = {};
    }

    ~Task() { cancel(); }

  private:
    Handle handle_{};
};

} // namespace Cory::Proper

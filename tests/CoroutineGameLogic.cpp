#include <catch2/catch_test_macros.hpp>
#include <gsl/gsl>

#include <Cory/Base/SimulationClock.hpp>
// TODO: one day, maybe write our own std::function replacement, see
// https://www.youtube.com/watch?v=xJSKk_q25oQ
#include <functional>

#include <cppcoro/coroutine.hpp>

#include <Cory/Base/FutureFrameQueue.hpp>
#include <Cory/Base/Log.hpp>

#include <range/v3/view/concat.hpp>

using namespace Cory;

class World;

/**
 * Behavior is a coroutine type that can be used to implement game logic.
 *
 * When entering the coroutine, the coroutine is executed until the first yield.
 *
 * The game world will the resume the coroutine on the next tick.
 *
 */
class Behavior {
  public:
    struct promise_type {
        Behavior get_return_object() { return Behavior{this}; }
        auto initial_suspend() { return cppcoro::suspend_never{}; }
        // suspend always so we can be explicit about destroying it
        auto final_suspend() noexcept { return cppcoro::suspend_always{}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }

        // adopted means the coroutine was moved to some other entity that manages it
        // and such we don't need to destroy it ourselves
        bool wasAdopted_{false};
    };

    using Handle = cppcoro::coroutine_handle<promise_type>;

    explicit Behavior(promise_type *p)
        : handle_{Handle::from_promise(*p)}
    {
    }
    // movable
    Behavior(Behavior &&rhs)
        : handle_{std::exchange(rhs.handle_, nullptr)}
    {
    }
    ~Behavior()
    {
        if (handle_ && !handle_.promise().wasAdopted_) { handle_.destroy(); }
    }

    Handle handle_;
};

class AnotherCoro {
  public:
    struct promise_type {
        AnotherCoro get_return_object() { return AnotherCoro{this}; }
        auto initial_suspend() { return cppcoro::suspend_never{}; }
        auto final_suspend() noexcept { return cppcoro::suspend_never{}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    using Handle = cppcoro::coroutine_handle<promise_type>;

    explicit AnotherCoro(promise_type *p)
        : handle_{Handle::from_promise(*p)}
    {
    }
    // movable
    AnotherCoro(AnotherCoro &&rhs)
        : handle_{std::exchange(rhs.handle_, nullptr)}
    {
    }
    ~AnotherCoro()
    {
        if (handle_) { handle_.destroy(); }
    }

    Handle handle_;
};

struct LogicComponent {
    std::function<Behavior(World &)> logic;
};

class World {
  public:
    World()
        : clock_{}
    {
    }

    void tick();
    void tickBy(Seconds duration);
    void end();

  public:
    friend class Behavior;

    auto sleepForTicks(uint64_t sleepTicks)
    {
        struct Awaiter {
            World &world_;
            uint64_t sleepTicks;
            bool await_ready() const noexcept { return false; }
            void await_suspend(Behavior::Handle h) noexcept
            {
                world_.waitingForFutureTicks_.enqueueFor(world_.lastTick_.ticks + sleepTicks, h);
                h.promise().wasAdopted_ = true;
            }
            SimulationClock::TickInfo await_resume() noexcept { return world_.lastTick_; }
        };
        return Awaiter{*this, sleepTicks};
    }
    auto sleepNextTick() { return sleepForTicks(1); }

    auto sleepFor(Seconds sleepTime)
    {
        struct Awaiter {
            World &world_;
            Seconds sleepTime;
            bool await_ready() const noexcept { return false; }
            void await_suspend(Behavior::Handle h) noexcept
            {
                world_.waitingForTimePoint_.enqueueFor(world_.lastTick_.now + sleepTime, h);
                h.promise().wasAdopted_ = true;
            }
            SimulationClock::TickInfo await_resume() noexcept { return world_.lastTick_; }
        };
        return Awaiter{*this, sleepTime};
    }

  private:
    void processTick(SimulationClock::TickInfo tickInfo);

    SimulationClock clock_{};
    SimulationClock::TickInfo lastTick_;

    FutureFrameQueue<uint64_t, Behavior::Handle> waitingForFutureTicks_;
    FutureFrameQueue<SimulationClock::time_point, Behavior::Handle> waitingForTimePoint_;
};

void World::tick()
{
    auto tickInfo = clock_.tick();
    processTick(tickInfo);
}
void World::tickBy(Seconds duration) {
    auto tickInfo = clock_.tickBy(duration);
    processTick(tickInfo);
}

void World::processTick(SimulationClock::TickInfo tickInfo)
{
    lastTick_ = tickInfo;
    // get all coroutines that are waiting for the next tick and resume them
    auto scheduledFromTicks = waitingForFutureTicks_.dequeueUntil(lastTick_.ticks);
    auto scheduledFromTimepoint = waitingForTimePoint_.dequeueUntil(lastTick_.now);
    for (auto h : ranges::view::concat(scheduledFromTicks, scheduledFromTimepoint)) {
        if (h.done()) { CO_CORE_WARN("Coroutine is already done!"); }
        else {
            h.resume();
        }
        if (h.done()) {
            CO_CORE_INFO("Coroutine is done! Destroying...");
            h.destroy();
        }
    };
}

void World::end()
{
    auto waiting = waitingForFutureTicks_.dequeueAll();
    for (auto h : waiting) {
        h.destroy();
    }
}

TEST_CASE("Simple behavior")
{
    int state{0};
    World world;

    Behavior ticker = [](World &world, int &state) -> Behavior {
        gsl::final_action cleanup{[&]() {
            state = -1;
            CO_CORE_INFO("behavior: cleanup");
        }};
        state = 1;
        CO_CORE_INFO("behavior: initialization");
        auto tick = co_await world.sleepNextTick();
        CO_CORE_INFO("behavior: tick 1: {:<05f}", tick.now.time_since_epoch().count());
        state = 2;
        auto tick2 = co_await world.sleepNextTick();
        CO_CORE_INFO("behavior: tick 2: {:<05f}", tick2.now.time_since_epoch().count());
        state = 3;
    }(world, state);

    LogicComponent logic{
        [](World &world) -> Behavior { auto tick = co_await world.sleepNextTick(); }};

    //    AnotherCoro ticker2 = [](World &world, int &state) -> AnotherCoro {
    //        state = 1;
    //        auto tick = co_await world.sleepNextTick();
    //    }(world, state);

    CHECK(state == 1);
    CO_CORE_INFO("Before world tick");
    world.tick();
    CO_CORE_INFO("World tick 1 complete");
    CHECK(state == 2);
    world.tick();
    CO_CORE_INFO("World tick 2 complete");
    CHECK(state == -1);
    world.tick();
    CO_CORE_INFO("World tick 3 complete");
    CHECK(state == -1);
}

TEST_CASE("Looping behavior")
{
    int state{0};
    World world;

    Behavior ticker = [](World &world, int &state) -> Behavior {
        gsl::final_action cleanup{[&]() {
            state = -1;
            CO_CORE_INFO("behavior: cleanup");
        }};
        while (true) {
            auto tick = co_await world.sleepNextTick();
            ++state;
        }
    }(world, state);

    CHECK(state == 0);
    CO_CORE_INFO("Before world tick");
    world.tick();
    CO_CORE_INFO("World tick 1 complete");
    CHECK(state == 1);
    world.tick();
    CO_CORE_INFO("World tick 2 complete");
    CHECK(state == 2);

    world.end();
    CO_CORE_INFO("World ended");
    CHECK(state == -1);
}

TEST_CASE("Sleeping multiple ticks")
{
    int state{0};
    World world;

    Behavior ticker = [](World &world, int &state) -> Behavior {
        gsl::final_action cleanup{[&]() {
            state = -1;
            CO_CORE_INFO("behavior: cleanup");
        }};
        state = 1;
        auto tick1 = co_await world.sleepForTicks(2);
        state = 2;
        auto tick2 = co_await world.sleepForTicks(2);
        state = 3;
    }(world, state);

    CHECK(state == 1);
    CO_CORE_INFO("Before world tick");
    world.tick();
    CO_CORE_INFO("World tick 1 complete");
    CHECK(state == 1);
    world.tick();
    CO_CORE_INFO("World tick 2 complete");
    CHECK(state == 2);
    world.tick();
    CO_CORE_INFO("World tick 3 complete");
    CHECK(state == 2);

    world.end();
    CO_CORE_INFO("World ended");
    CHECK(state == -1);
}

TEST_CASE("Sleeping for simulated time")
{
    using namespace Cory::literals;
    int state{0};
    World world;

    Behavior ticker = [](World &world, int &state) -> Behavior {
        gsl::final_action cleanup{[&]() {
            state = -1;
            CO_CORE_INFO("behavior: cleanup");
        }};
        state = 1;
        auto tick1 = co_await world.sleepFor(2.0_ms);
        state = 2;
    }(world, state);

    CHECK(state == 1);
    CO_CORE_INFO("Before world tick");
    world.tick();
    CO_CORE_INFO("World tick 1 complete");
    CHECK(state == 1);
    world.tick();
    CO_CORE_INFO("World tick 2 complete");
    CHECK(state == 1);
    world.tick();
    CO_CORE_INFO("World tick 3 complete");
    CHECK(state == 2);

    world.end();
    CO_CORE_INFO("World ended");
    CHECK(state == -1);
}
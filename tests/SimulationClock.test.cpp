#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/SimulationClock.hpp>

static_assert(std::chrono::is_clock_v<Cory::SimulationClock>);

class MockClock {
  public:
    static constexpr bool is_steady{false};
    using duration = std::chrono::duration<double>;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<MockClock>;

    static time_point now() { return s_now; }
    static void setNow(duration now) { s_now = time_point{} + now; }
    static void setNow(time_point now) { s_now = now; }

  private:
    static time_point s_now;
};
MockClock::time_point MockClock::s_now{};

TEST_CASE("Clock")
{
    using namespace Cory::literals;
    auto set_now = [](double now) { MockClock::setNow(MockClock::duration{now}); };

    using TestClock = Cory::BasicSimulationClock<MockClock>;
    auto tp = [](double t) { return TestClock::time_point{} + TestClock::duration{t}; };

    set_now(3.0);
    TestClock clk;
    // check that the clock starts at 0
    auto tick0 = clk.lastTick();
    CHECK(tick0 == TestClock::TickInfo{});

    auto tick1 = clk.tick();
    CHECK(tick1.now == tp(0.0));
    CHECK(tick1.realNow == tp(0.0));
    CHECK(tick1.delta == 0.0_s);
    CHECK(tick1.realDelta == 0.0_s);
    CHECK(tick1.ticks == 1);
    CHECK(tick1 == clk.lastTick());

    set_now(4.0);
    auto tick2 = clk.tick();
    CHECK(tick2.now == tp(1.0));
    CHECK(tick2.realNow == tp(1.0));
    CHECK(tick2.delta == 1.0_s);
    CHECK(tick2.realDelta == 1.0_s);
    CHECK(tick2.ticks == 2);
    CHECK(tick2 == clk.lastTick());

    set_now(6.0);
    auto tick3 = clk.tick();
    CHECK(tick3.now == tp(3.0));
    CHECK(tick3.realNow == tp(3.0));
    CHECK(tick3.delta == 2.0_s);
    CHECK(tick3.realDelta == 2.0_s);
    CHECK(tick3.ticks == 3);
    CHECK(tick3 == clk.lastTick());

    clk.setTimeScale(2.0);
    set_now(7.0);
    auto tick4 = clk.tick();
    CHECK(tick4.now == tp(5.0));
    CHECK(tick4.realNow == tp(4.0));
    CHECK(tick4.delta == 2.0_s);
    CHECK(tick4.realDelta == 1.0_s);
    CHECK(tick4.ticks == 4);
    CHECK(tick4 == clk.lastTick());

    clk.reset();
    CHECK(clk.lastTick() == TestClock::TickInfo{});

    set_now(10.0);
    auto tick5 = clk.tick();
    CHECK(tick5.now == tp(6.0));
    CHECK(tick5.realNow == tp(3.0));
    CHECK(tick5.delta == 6.0_s);
    CHECK(tick5.realDelta == 3.0_s);
    CHECK(tick5.ticks == 1);
    CHECK(tick5 == clk.lastTick());
}
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
    CHECK(clk.simNow() == tp(0.0));
    CHECK(clk.realNow() == tp(0.0));
    CHECK(clk.ticks() == 0);

    clk.tick();
    CHECK(clk.simNow() == tp(0.0));
    CHECK(clk.realNow() == tp(0.0));
    CHECK(clk.ticks() == 1);

    set_now(4.0);
    clk.tick();
    CHECK(clk.simNow() == tp(1.0));
    CHECK(clk.realNow() == tp(1.0));
    CHECK(clk.ticks() == 2);

    set_now(6.0);
    clk.tick();
    CHECK(clk.simNow() == tp(3.0));
    CHECK(clk.realNow() == tp(3.0));
    CHECK(clk.ticks() == 3);

    clk.setTimeScale(2.0);
    set_now(7.0);
    clk.tick();
    CHECK(clk.simNow() == tp(5.0));
    CHECK(clk.realNow() == tp(4.0));
    CHECK(clk.ticks() == 4);

    clk.reset();
    CHECK(clk.simNow() == tp(0.0));
    CHECK(clk.realNow() == tp(0.0));
    CHECK(clk.ticks() == 0);

    set_now(10.0);
    clk.tick();
    CHECK(clk.simNow() == tp(6.0));
    CHECK(clk.realNow() == tp(3.0));
    CHECK(clk.ticks() == 1);

}
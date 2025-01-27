#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/SignalTree.hpp>

#include <barrier>
#include <random>
#include <set>
#include <thread>

namespace std {
class latch;
}
TEST_CASE("SignalTree", "[Cory/Base]")
{
    SECTION("Initializes with a power of two")
    {
        CHECK_NOTHROW(Cory::SignalTree(2));
        CHECK_NOTHROW(Cory::SignalTree(8));
        CHECK_THROWS(Cory::SignalTree(7));
        CHECK_THROWS(Cory::SignalTree(123));
    }

    SECTION("Setting and querying a signal")
    {
        Cory::SignalTree signals(8);

        signals.set(1);
        signals.validateInternal();

        REQUIRE(signals.unsafeQueryIsSet(1));
        signals.set(3);

        REQUIRE(signals.unsafeQueryIsSet(3));

        REQUIRE(signals.count() == 2);
    }
    SECTION("Setting all signals")
    {
        Cory::SignalTree signals(8);

        for (int i = 0; i < 8; ++i) {
            signals.set(i);
            signals.validateInternal();
        }

        REQUIRE(signals.count() == 8);
    }
    SECTION("clearing a signal slot when no signals are set")
    {
        Cory::SignalTree signals(8);

        REQUIRE_FALSE(signals.clearNext().has_value());
    }

    SECTION("Setting a single signal and then clearing it")
    {
        Cory::SignalTree signals(8);

        for (int i = 0; i < 8; ++i) {
            signals.set(i);
            auto cleared = signals.clearNext();
            REQUIRE(cleared.has_value());
            REQUIRE(cleared.value() == i);
            signals.validateInternal();
        }
    }

    SECTION("Setting a random set of signals and then querying them")
    {
        Cory::SignalTree signals(256);

        // generate a random set of signals
        std::set<size_t> signalsToSet;
        for (int i = 0; i < 256; ++i) {
            if (rand() % 3) {
                signalsToSet.insert(i);
                signals.set(i);
                signals.validateInternal();
            }
        }

        std::set<size_t> signalsThatWereSet;
        for (auto signal = signals.clearNext(); signal.has_value(); signal = signals.clearNext()) {
            signalsThatWereSet.insert(signal.value());
            signals.validateInternal();
        }

        REQUIRE(signalsToSet == signalsThatWereSet);
    }
}

TEST_CASE("SignalTree MT Stress/Fuzz", "[Cory/Base]")
{
    // This test creates a number of producers, each of which have their own signal subset assigned.
    // On each iteration, the producers set their signals and then synchronize at a shared barrier.
    // The consumer clears all signals and then kicks off another iteration. At the end, we make
    // sure that each signal was invoked once per iteration if it was assigned to a thread.
    static constexpr auto MAX_SIGNALS = 2 << 18;
    static constexpr auto SIGNALS_PER_THREAD = 2 << 13;
    static constexpr auto NUM_PRODUCERS = 16;
    static constexpr auto NUM_CONSUMERS = 2;
    static constexpr auto NUM_ITERATIONS = 100;

    Cory::SignalTree signals(MAX_SIGNALS);

    // Generate a random set of available signal indices
    std::vector<Cory::SignalTree::SignalIdx> signalIndices(MAX_SIGNALS);
    {
        std::random_device rd;
        std::mt19937 g(rd());
        std::iota(signalIndices.begin(), signalIndices.end(), 0);
        std::shuffle(signalIndices.begin(), signalIndices.end(), g);
    }

    // one latch per iteration, to synchronize all producers finishing their loop
    std::barrier iteration_barrier(NUM_PRODUCERS + NUM_CONSUMERS);
    std::barrier consumers_done(NUM_PRODUCERS + NUM_CONSUMERS);
    std::atomic<size_t> producersActive{0};

    // Every producer gets its individual slice of the signal indices
    auto producer_func = [&](size_t indexOffset) {
        return [&, indexOffset]() {
            std::vector<Cory::SignalTree::SignalIdx> thisThreadSignals{
                signalIndices.begin() + indexOffset,
                signalIndices.begin() + indexOffset + SIGNALS_PER_THREAD};

            std::random_device rd;
            std::mt19937 g(rd());

            for (int i = 0; i < NUM_ITERATIONS; ++i) {
                ++producersActive;
                for (auto signal : thisThreadSignals) {
                    signals.set(signal);
                }
                --producersActive;
                iteration_barrier.arrive_and_wait();

                // re-shuffle the signals for the next iteration
                std::shuffle(thisThreadSignals.begin(), thisThreadSignals.end(), g);
                consumers_done.arrive_and_wait();
            }
        };
    };

    std::vector<std::vector<size_t>> signalsInvokedCounters{};
    signalsInvokedCounters.resize(NUM_CONSUMERS);
    auto consumer_func = [&](size_t consumerId) {
        return [&, consumerId]() {
            auto &signalsInvoked = signalsInvokedCounters[consumerId];
            signalsInvoked.resize(MAX_SIGNALS, 0);
            for (int i = 0; i < NUM_ITERATIONS; ++i) {

                while (producersActive.load() > 0) {
                    auto signal = signals.clearNext();
                    if (signal.has_value()) { signalsInvoked[signal.value()]++; }
                }
                // arrive at the barrier and do some sanity checking
                iteration_barrier.arrive_and_wait();

                // all producers should now be done for this iteration, so we can do some
                // single-threaded validity checks
                if (consumerId == 0) { signals.validateInternal(); }

                // kick off the next round
                consumers_done.arrive_and_wait();
            }
        };
    };

    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back(producer_func(i * SIGNALS_PER_THREAD));
    }
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back(consumer_func(i));
    }

    for (auto &consumer : consumers) {
        consumer.join();
    }
    for (auto &producer : producers) {
        producer.join();
    }

    for (int i = 0; i < MAX_SIGNALS; ++i) {
        auto signal_idx = signalIndices[i];
        auto signal_invoked = std::accumulate(
            signalsInvokedCounters.begin(),
            signalsInvokedCounters.end(),
            0,
            [signal_idx](size_t sum, const std::vector<size_t> &v) { return sum + v[signal_idx]; });

        if (i < NUM_PRODUCERS * SIGNALS_PER_THREAD) { CHECK(signal_invoked == NUM_ITERATIONS); }
        else {
            CHECK(signal_invoked == 0);
        }
    }
}
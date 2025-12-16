#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/SignalTree.hpp>

#include <spdlog/spdlog.h>

#include <barrier>
#include <latch>
#include <random>
#include <set>
#include <thread>

TEST_CASE("SignalTree", "[Cory/SignalTree]")
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

        REQUIRE_FALSE(signals.select().has_value());
    }

    SECTION("Setting a single signal and then clearing it")
    {
        Cory::SignalTree signals(8);

        for (int i = 0; i < 8; ++i) {
            signals.set(i);
            auto cleared = signals.select();
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

        uint64_t biasFlags = rand();
        std::set<size_t> signalsThatWereSet;
        for (auto signal = signals.select(++biasFlags); signal.has_value();
             signal = signals.select()) {
            signalsThatWereSet.insert(signal.value());
            signals.validateInternal();
        }

        REQUIRE(signalsToSet == signalsThatWereSet);
    }

    SECTION("Creating fully signaled")
    {
        auto num_signals = 4096ull;
        Cory::SignalTree signals(num_signals, Cory::SignalTree::CreateMode::FullySignaled);
        REQUIRE(signals.count() == num_signals);
        signals.validateInternal();

        std::vector<uint64_t> signalsSet;
        for (size_t i = 0; i < num_signals; ++i) {
            REQUIRE(signals.unsafeQueryIsSet(i));
            signalsSet.push_back(signals.select().value());
        }
        REQUIRE(signalsSet.size() == num_signals);

        // all signals from 0 to 63 were set
        std::sort(signalsSet.begin(), signalsSet.end());
        for (gsl::index i = 0; i < num_signals; ++i) {
            REQUIRE(signalsSet[i] == i);
        }
    }
}

TEST_CASE("SignalTree MT Producer Only", "[Cory/SignalTree]")
{
    struct SignalTreeTestConfig {
        uint64_t SIGNALS_PER_THREAD;
        uint64_t NUM_PRODUCERS;
        uint64_t MAX_SIGNALS;
    };
    auto run_producer_only_test = [](SignalTreeTestConfig cfg) {
        Cory::SignalTree signals(cfg.MAX_SIGNALS);
        std::latch testStartLatch(1);

        // Generate a random set of available signal indices
        std::vector<Cory::SignalTree::SignalIdx> signalIndices(cfg.MAX_SIGNALS);
        {
            std::random_device rd;
            std::mt19937 g(rd());
            std::iota(signalIndices.begin(), signalIndices.end(), 0);
            std::shuffle(signalIndices.begin(), signalIndices.end(), g);
        }

        // Every producer gets its individual slice of the signal indices
        auto producer_func = [&](size_t indexOffset) {
            return [&, indexOffset]() {
                std::vector<Cory::SignalTree::SignalIdx> thisThreadSignals{
                    signalIndices.begin() + indexOffset,
                    signalIndices.begin() + indexOffset + cfg.SIGNALS_PER_THREAD};

                testStartLatch.wait();
                for (auto signal : thisThreadSignals) {
                    signals.set(signal);
                }
            };
        };

        std::vector<std::thread> producers;
        for (int i = 0; i < cfg.NUM_PRODUCERS; ++i) {
            producers.emplace_back(producer_func(i * cfg.SIGNALS_PER_THREAD));
        }

        // start all producers at the same time to create a bit more contention
        testStartLatch.count_down();

        for (auto &producer : producers) {
            producer.join();
        }
        // signals count must match
        REQUIRE(signals.count() == (cfg.NUM_PRODUCERS * cfg.SIGNALS_PER_THREAD));
        // signal tree must be internally consistent
        signals.validateInternal();

        // The correct signals must be set
        for (uint64_t i = 0; i < cfg.MAX_SIGNALS; ++i) {
            auto signalIdx = signalIndices[i];
            bool shouldBeSet = i < cfg.NUM_PRODUCERS * cfg.SIGNALS_PER_THREAD;
            // CAPTURE(i);
            // CAPTURE(signalIdx);
            // spdlog::critical(signals.debugPrint());
            REQUIRE(signals.unsafeQueryIsSet(signalIdx) == shouldBeSet);
        }
    };

    SECTION("Basic - MT Set all")
    {
        run_producer_only_test({
            .SIGNALS_PER_THREAD = 4,
            .NUM_PRODUCERS = 32,
            .MAX_SIGNALS = 4 * 32,
        });
    }
    SECTION("Basic - MT Set some")
    {
        run_producer_only_test({
            .SIGNALS_PER_THREAD = 2,
            .NUM_PRODUCERS = 16,
            .MAX_SIGNALS = 64,
        });
    }
    SECTION("Basic - Very MT Set some")
    {
        run_producer_only_test({
            .SIGNALS_PER_THREAD = 4096,
            .NUM_PRODUCERS = 16,
            .MAX_SIGNALS = 4096 * 4096,
        });
    }
    SECTION("Basic - MT Set HALF")
    {
        run_producer_only_test({
            .SIGNALS_PER_THREAD = 1024,
            .NUM_PRODUCERS = 1024,
            .MAX_SIGNALS = 2 * 1024 * 1024,
        });
    }
    SECTION("Basic - Very MT Set all")
    {
        run_producer_only_test({
            .SIGNALS_PER_THREAD = 4096,
            .NUM_PRODUCERS = 4096,
            .MAX_SIGNALS = 4096 * 4096,
        });
    }
}

TEST_CASE("SignalTree MT Stress/Fuzz", "[Cory/SignalTree]")
{
    // This test creates a number of producers, each of which have their own signal subset assigned.
    // On each iteration, the producers set their signals and then synchronize at a shared barrier.
    // The consumer clears all signals and then kicks off another iteration. At the end, we make
    // sure that each signal was invoked once per iteration if it was assigned to a thread.

    struct SignalTreeTestConfig {
        uint64_t MAX_SIGNALS;
        uint64_t SIGNALS_PER_THREAD;
        uint64_t NUM_PRODUCERS;
        uint64_t NUM_CONSUMERS;
        uint64_t NUM_ITERATIONS;
    };

    auto run_mt_stress_test = [](SignalTreeTestConfig cfg) {
        Cory::SignalTree signals(cfg.MAX_SIGNALS);

        // Generate a random set of available signal indices
        std::vector<Cory::SignalTree::SignalIdx> signalIndices(cfg.MAX_SIGNALS);
        {
            std::random_device rd;
            std::mt19937 g(rd());
            std::iota(signalIndices.begin(), signalIndices.end(), 0);
            std::shuffle(signalIndices.begin(), signalIndices.end(), g);
        }

        // one latch per iteration, to synchronize all producers finishing their loop
        std::barrier iteration_barrier(cfg.NUM_PRODUCERS + cfg.NUM_CONSUMERS);
        std::barrier consumers_done(cfg.NUM_PRODUCERS + cfg.NUM_CONSUMERS);
        std::atomic<size_t> producersActive{0};

        // Every producer gets its individual slice of the signal indices
        auto producer_func = [&](size_t indexOffset) {
            return [&, indexOffset]() {
                std::vector<Cory::SignalTree::SignalIdx> thisThreadSignals{
                    signalIndices.begin() + indexOffset,
                    signalIndices.begin() + indexOffset + cfg.SIGNALS_PER_THREAD};

                std::random_device rd;
                std::mt19937 g(rd());

                for (int i = 0; i < cfg.NUM_ITERATIONS; ++i) {
                    ++producersActive;
                    for (auto signal : thisThreadSignals) {
                        bool wasSet = signals.set(signal);
                        // if (!wasSet) {
                        //     CAPTURE(signal);
                        //     REQUIRE(wasSet);
                        // }
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
        signalsInvokedCounters.resize(cfg.NUM_CONSUMERS);
        auto consumer_func = [&](size_t consumerId) {
            return [&, consumerId]() {
                uint64_t bias = consumerId;
                auto &signalsInvoked = signalsInvokedCounters[consumerId];
                signalsInvoked.resize(cfg.MAX_SIGNALS, 0);
                auto drain_signals = [&]() {
                    for (auto signal = signals.select(consumerId); signal.has_value();
                         signal = signals.select(consumerId)) {
                        signalsInvoked[signal.value()]++;
                        ++bias;
                    }
                };
                for (int i = 0; i < cfg.NUM_ITERATIONS; ++i) {
                    while (producersActive > 0) {
                        drain_signals();
                    }

                    // arrive at the barrier and do some sanity checking
                    iteration_barrier.arrive_and_wait();

                    // producers have stopped setting signals, so we can drain the rest
                    drain_signals();

                    // all producers should now be done for this iteration, so we can do some
                    // single-threaded validity checks
                    if (consumerId == 0) {
                        // non-zero signals would indicate the consumers haven't done their job
                        // CAPTURE(signals.debugPrint());
                        REQUIRE(signals.count() == 0);

                        try {
                            signals.validateInternal();
                        }
                        catch (const std::exception &e) {
                            spdlog::critical(e.what());
                            spdlog::shutdown();
                            FAIL("Validation failed");
                        }
                    }

                    // kick off the next round
                    consumers_done.arrive_and_wait();
                }
            };
        };

        std::vector<std::thread> producers;
        for (int i = 0; i < cfg.NUM_PRODUCERS; ++i) {
            producers.emplace_back(producer_func(i * cfg.SIGNALS_PER_THREAD));
        }
        std::vector<std::thread> consumers;
        for (int i = 0; i < cfg.NUM_CONSUMERS; ++i) {
            consumers.emplace_back(consumer_func(i));
        }

        for (auto &consumer : consumers) {
            consumer.join();
        }
        for (auto &producer : producers) {
            producer.join();
        }

        for (int i = 0; i < cfg.MAX_SIGNALS; ++i) {
            auto signal_idx = *signalIndices[i];
            uint64_t signal_invoked =
                std::accumulate(signalsInvokedCounters.begin(),
                                signalsInvokedCounters.end(),
                                0ull,
                                [signal_idx](size_t sum, const std::vector<size_t> &v) {
                                    return sum + v[signal_idx];
                                });

            if (i < cfg.NUM_PRODUCERS * cfg.SIGNALS_PER_THREAD) {
                CHECK(signal_invoked == cfg.NUM_ITERATIONS);
            }
            else {
                CHECK(signal_invoked == 0);
            }
        }
    };

    SECTION("SPSC Test")
    {
        run_mt_stress_test({.MAX_SIGNALS = 32,
                            .SIGNALS_PER_THREAD = 1,
                            .NUM_PRODUCERS = 1,
                            .NUM_CONSUMERS = 1,
                            .NUM_ITERATIONS = 100});
    }
    SECTION("MPMC Small")
    {
        run_mt_stress_test({.MAX_SIGNALS = 32,
                            .SIGNALS_PER_THREAD = 1,
                            .NUM_PRODUCERS = 16,
                            .NUM_CONSUMERS = 2,
                            .NUM_ITERATIONS = 100});
    }
    SECTION("MPMC Medium")
    {
        run_mt_stress_test({.MAX_SIGNALS = 1ull << 14,
                            .SIGNALS_PER_THREAD = 1ull << 9,
                            .NUM_PRODUCERS = 16,
                            .NUM_CONSUMERS = 2,
                            .NUM_ITERATIONS = 100});
    }
    // SECTION("MPMC Large")
    //{
    //     run_mt_stress_test({.MAX_SIGNALS = 1ull << 16,
    //                         .SIGNALS_PER_THREAD = 1ull << 11,
    //                         .NUM_PRODUCERS = 16,
    //                         .NUM_CONSUMERS = 16,
    //                         .NUM_ITERATIONS = 100});
    // }
}
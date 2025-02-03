#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/WorkContractGroup.hpp>

TEST_CASE("WorkContractGroup", "Cory/Base/WorkContractGroup")
{
    Cory::WorkContractGroup workContractGroup(256);

    WHEN("Nothing has happened")
    {
        THEN("Executing the next contract does nothing")
        {
            REQUIRE_FALSE(workContractGroup.executeNext());
        }
    }
    WHEN("Creating a work contract")
    {
        uint64_t counter = 0;

        Cory::WorkContract contract = workContractGroup.createContract([&] { ++counter; });

        THEN("The contract is valid") { CHECK(contract.valid()); }

        AND_WHEN("Attempting to execute it")
        {
            THEN("Nothing is executed")
            {
                CHECK(workContractGroup.contractsScheduled() == 0);
                CHECK_FALSE(workContractGroup.executeNext());
                CHECK(workContractGroup.contractsScheduled() == 0);
                CHECK(counter == 0);
            }
        }
        AND_WHEN("Scheduling and then executing it")
        {
            contract.schedule();

            THEN("The work is executed")
            {
                CHECK(workContractGroup.contractsScheduled() == 1);
                CHECK(workContractGroup.executeNext());
                CHECK(workContractGroup.contractsScheduled() == 0);
                CHECK(counter == 1);

                AND_WHEN("Exeucting more tasks")
                {
                    THEN("Nothing happens")
                    {
                        CHECK(workContractGroup.contractsScheduled() == 0);
                        CHECK_FALSE(workContractGroup.executeNext());
                        CHECK(workContractGroup.contractsScheduled() == 0);
                        CHECK(counter == 1);
                    }
                }

                AND_WHEN("Re-scheduling the work")
                {
                    contract.schedule();

                    THEN("It is executed")
                    {
                        CHECK(workContractGroup.contractsScheduled() == 1);
                        CHECK(workContractGroup.executeNext());
                        CHECK(workContractGroup.contractsScheduled() == 0);
                        CHECK(counter == 2);
                    }
                }
            }
        }
    }
    WHEN("Creating many contracts")
    {
        struct ContractState {
            uint64_t counter = 0;
            bool executed = false;
        };
        std::vector<Cory::WorkContract> contracts;
        std::vector<ContractState> states(256);
        for (size_t i = 0; i < 256; ++i) {
            contracts
                .emplace_back(workContractGroup.createContract([&state = states[i]] {
                    ++state.counter;
                    state.executed = true;
                }))
                .schedule();
        }
        THEN("All contracts are valid")
        {
            for (auto &contract : contracts) {
                CHECK(contract.valid());
            }
        }

        THEN("The work group has all contracts scheduled")
        {
            CHECK(workContractGroup.contractsCreated() == states.size());
            CHECK(workContractGroup.contractsScheduled() == states.size());
        }
        AND_WHEN("The work contract group is drained")
        {
            uint64_t contractsExecuted = 0;
            while (workContractGroup.executeNext()) {
                ++contractsExecuted;
            }
            THEN("All contracts are executed")
            {
                CHECK(contractsExecuted == states.size());
                CHECK(workContractGroup.contractsScheduled() == 0);
                for (auto &state : states) {
                    CHECK(state.executed);
                }
            }
        }
    }
}
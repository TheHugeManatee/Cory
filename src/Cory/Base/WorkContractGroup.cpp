#include <Cory/Base/WorkContractGroup.hpp>

#include <Cory/Base/Log.hpp>

#define CO_WORKCONTRACT_ASSERT(cond, msg) CO_CORE_ASSERT(cond, msg)

namespace Cory {

WorkContract::~WorkContract()
{
    if (id_.has_value()) {
        unschedule();
        // return the ID to the available pool
        group_->contractIdsAvailable_.set(*id_);
    }
}

void WorkContract::schedule()
{
    CO_WORKCONTRACT_ASSERT(valid(), "Cannot schedule an invalid contract");
    auto &contract = group_->contracts_[*id_];
    // TODO handle rescheduling propertly
    //    - if the contract is already scheduled, we should not schedule it again
    //    - if the contract is currently executing, we should not schedule it again

    // scheduled by setting the signal ID in the scheduled tree
    group_->contractsScheduled_.set(*id_);
}

void WorkContract::unschedule()
{
    // TODO? Do we want/need this at all?
}

WorkContract::WorkContract(WorkContractId id, WorkContractGroup *group)
    : id_(id)
    , group_(group)
{
}

WorkContractGroup::WorkContractGroup(size_t capacity)
    : contractsScheduled_(capacity)
    , contractIdsAvailable_(capacity, SignalTree::CreateMode::FullySignaled)
    , contracts_(capacity)
{
}

bool WorkContractGroup::executeNext(uint64_t biasBits)
{
    auto id = contractsScheduled_.select(biasBits);
    if (!id.has_value()) {
        return false;
    }

    auto &contract = contracts_[*id];

    contract.setBits(ContractFlagExecuting);

    ContractToken token{contract};
    contract.work(token);

    // handle re-scheduling via token.schedule() method:
    //  - atomically check and clear the ScheduleRequested flag
    //  - if it was set, we need to reschedule the contract
    //  NB: We could directly re-execute without rescheduling, but this would
    //    lead to recursion as well as very unfair scheduling of contracts.
    if (auto prev_flags = contract.clearBits(ContractFlagExecuting | ContractFlagScheduleRequested |
                                             ContractFlagScheduled);
        prev_flags & ContractFlagScheduleRequested) {

        // still needs to be atomic, as another thread could have scheduled it in the meantime
        if (!contract.setBits(ContractFlagScheduled)) {
            contractsScheduled_.set(*id);
        }
    }

    return true;
}

WorkContract WorkContractGroup::createContractInternal(ContractFunctor &&work)
{
    auto id = contractIdsAvailable_.select(rand());

    if (!id.has_value()) {
        // No more contract slots available! :(
        return WorkContract{};
    }

    auto &contract = contracts_[*id];
    contract.contractFlags = 0;
    contract.work = std::move(work);

    return WorkContract{*id, this};
}

} // namespace Cory
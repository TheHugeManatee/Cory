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
    if (!id.has_value()) { return false; }

    auto &contract = contracts_[*id];

    ContractToken token{};
    contract.work(token);

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
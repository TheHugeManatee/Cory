#pragma once

#include <Cory/Base/SignalTree.hpp>
#include <Cory/Base/ValOptional.hpp>

namespace Cory {

using WorkContractId = ValOptional<uint64_t>;
/// Contract tokens can be passed to a contract's functor to provide an
/// interface to reschedule itself.
class ContractToken;
class WorkContract;
class WorkContractGroup;

class ContractFunctor {
    struct CallableBase {
        virtual void invoke(ContractToken &arg) = 0;
        virtual ~CallableBase() = default;
    };

    template <typename F> struct CallableImpl : CallableBase {
        F f;

        CallableImpl(F &&function)
            : f(std::forward<F>(function))
        {
        }

        void invoke(ContractToken &arg) override
        {
            if constexpr (requires { f(arg); }) {
                f(arg);
            }
            else {
                f();
            }
        }
    };

    std::unique_ptr<CallableBase> callable{};

  public:
    template <typename F>
    ContractFunctor(F &&f)
        : callable(std::make_unique<CallableImpl<std::decay_t<F>>>(std::forward<F>(f)))
    {
    }

    ContractFunctor() = default;

    void operator()(ContractToken &arg) { return callable->invoke(arg); }
};

class WorkContract {
  public:
    WorkContract() = default;
    explicit WorkContract(WorkContract &&rhs) noexcept
        : id_{std::exchange(rhs.id_, {})}
        , group_{rhs.group_}
    {
    }

    WorkContract &operator=(WorkContract &&rhs) noexcept
    {
        id_ = std::exchange(rhs.id_, {});
        group_ = rhs.group_;
        return *this;
    }

    ~WorkContract();

    void schedule();
    void unschedule();

    auto valid() const { return id_.has_value(); }

  private:
    // Only WorkContractGroup can create WorkContracts
    friend WorkContractGroup;

    WorkContract(WorkContractId id, WorkContractGroup *group);

    WorkContractId id_{};
    WorkContractGroup *group_{};
};

class WorkContractGroup {
  public:
    explicit WorkContractGroup(size_t capacity);

    template <typename Work>
    WorkContract createContract(Work &&work)
        requires(std::invocable<Work, ContractToken &> || std::invocable<Work>);

    /// Execute the next available work contract.
    bool executeNext(uint64_t biasBits = 0);

    size_t capacity() const { return contracts_.size(); }
    size_t contractsCreated() const { return capacity() - contractIdsAvailable_.count(); }
    size_t contractsScheduled() const { return contractsScheduled_.count(); }

  private:
    friend WorkContract;  // friended so it can schedule/unschedule itself
    friend ContractToken; // friended so it can receive a reference to Contract
    WorkContract createContractInternal(ContractFunctor &&work);

    static constexpr uint64_t ContractFlagScheduled = 1 << 0;
    static constexpr uint64_t ContractFlagScheduleRequested = 1 << 1;
    static constexpr uint64_t ContractFlagExecuting = 1 << 2;

    SignalTree contractsScheduled_;
    SignalTree contractIdsAvailable_;

    struct Contract {
        std::atomic<uint64_t> contractFlags{0};
        ContractFunctor work{};

        uint64_t setBits(uint64_t flag)
        {
            auto previous = contractFlags.fetch_or(flag);
            return previous & flag;
        }
        uint64_t clearBits(uint64_t flag)
        {
            auto previous = contractFlags.fetch_and(~flag);
            return previous & flag;
        }
    };
    std::vector<Contract> contracts_;
};

class ContractToken : NoCopy, NoMove {
  public:
    void schedule() { contract_.setBits(WorkContractGroup::ContractFlagScheduleRequested); }

  private:
    friend WorkContractGroup;

    ContractToken(WorkContractGroup::Contract &contract)
        : contract_(contract)
    {
    }

    WorkContractGroup::Contract &contract_;
};

template <typename Work>
WorkContract WorkContractGroup::createContract(Work &&work)
    requires(std::invocable<Work, ContractToken &> || std::invocable<Work>)
{
    return createContractInternal(ContractFunctor{std::forward<Work>(work)});
}

} // namespace Cory

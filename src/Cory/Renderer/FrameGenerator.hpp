#pragma once

#include <Cory/Renderer/FrameContext.hpp>

#include <cppcoro/generator.hpp>

#include <functional>
#include <iterator>
#include <utility>

namespace Cory {

class FrameGenerator {
  public:
    using Generator = cppcoro::generator<FrameContext>;

    class iterator {
      public:
        using GenIter = Generator::iterator;
        using GenSentinel = decltype(std::declval<Generator &>().end());

        iterator() = default;
        iterator(GenIter iter,
                 GenSentinel end,
                 std::function<void(FrameContext &)> *presenter);
        iterator(const iterator &) = delete;
        iterator &operator=(const iterator &) = delete;
        iterator(iterator &&) = default;
        iterator &operator=(iterator &&) = default;
        ~iterator();

        FrameContext &operator*() const;
        FrameContext *operator->() const;
        iterator &operator++();
        bool operator==(GenSentinel) const;
        bool operator!=(GenSentinel) const;

      private:
        void presentIfPending();

        GenIter iter_{};
        GenSentinel end_{};
        std::function<void(FrameContext &)> *presenter_{};
        bool pending_{false};
    };

    FrameGenerator(Generator generator, std::function<void(FrameContext &)> presenter);

    iterator begin();
    iterator::GenSentinel end() const;

  private:
    Generator generator_;
    std::function<void(FrameContext &)> presenter_;
};

} // namespace Cory

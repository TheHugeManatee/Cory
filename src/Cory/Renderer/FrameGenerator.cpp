#include <Cory/Renderer/FrameGenerator.hpp>

#include <memory>
#include <utility>

namespace Cory {

FrameGenerator::iterator::iterator(GenIter iter,
                                   GenSentinel end,
                                   std::function<void(FrameContext &)> *presenter)
    : iter_(std::move(iter)), end_(end), presenter_(presenter)
{
    pending_ = iter_ != end_;
}

FrameGenerator::iterator::~iterator()
{
    presentIfPending();
}

FrameContext &FrameGenerator::iterator::operator*() const
{
    return *iter_;
}

FrameContext *FrameGenerator::iterator::operator->() const
{
    return std::addressof(*iter_);
}

FrameGenerator::iterator &FrameGenerator::iterator::operator++()
{
    presentIfPending();
    ++iter_;
    pending_ = iter_ != end_;
    return *this;
}

bool FrameGenerator::iterator::operator==(GenSentinel) const
{
    return iter_ == end_;
}

bool FrameGenerator::iterator::operator!=(GenSentinel) const
{
    return iter_ != end_;
}

void FrameGenerator::iterator::presentIfPending()
{
    if (!pending_ || presenter_ == nullptr) return;
    (*presenter_)(*iter_);
    pending_ = false;
}

FrameGenerator::FrameGenerator(Generator generator, std::function<void(FrameContext &)> presenter)
    : generator_(std::move(generator)), presenter_(std::move(presenter))
{
}

FrameGenerator::iterator FrameGenerator::begin()
{
    return iterator{generator_.begin(), generator_.end(), &presenter_};
}

FrameGenerator::iterator::GenSentinel FrameGenerator::end() const
{
    return {};
}

} // namespace Cory

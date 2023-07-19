#pragma once

namespace Cory {

class SceneGraph;

using Entity = uint32_t;

template <typename T>
concept Component = std::copyable<T>;

class System;
template <typename Derived, Component... Cmps> class SimpleSystem;

} // namespace Cory
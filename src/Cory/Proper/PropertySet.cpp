#include "PropertySet.hpp"

#include <algorithm>
#include <type_traits>

namespace Cory::Prop {

PropertyHandle PropertySet::createImpl(const std::string &name, Property property)
{
    const auto handle = PropertyHandle{properties_.insert(std::move(property))};
    propertyLookup_[name] = handle;
    return handle;
}

PropertyHandle PropertySet::create(const std::string &name, PropertyValue value)
{
    return std::visit(
        [&](const auto &typedValue) -> PropertyHandle {
            using ValueType = std::decay_t<decltype(typedValue)>;
            if constexpr (std::same_as<ValueType, int>) {
                return create(name, Int{.value = typedValue});
            }
            else if constexpr (std::same_as<ValueType, float>) {
                return create(name, Float{.value = typedValue});
            }
            else if constexpr (std::same_as<ValueType, glm::vec3>) {
                return create(name, Vec3{.value = typedValue});
            }
            else if constexpr (std::same_as<ValueType, std::string>) {
                return create(name, String{.value = typedValue});
            }
        },
        value);
}

GroupHandle PropertySet::addGroup(const std::string &name)
{
    const auto handle = GroupHandle{groups_.insert(Group{})};
    groupLookup_[name] = handle;
    return handle;
}

void PropertySet::update(PropertyHandle handle, PropertyValue value)
{
    auto &property = properties_[handle];
    std::visit(
        [&](auto &typedProperty) {
            using PropertyType = std::decay_t<decltype(typedProperty)>;
            std::visit(
                [&](const auto &typedValue) {
                    using ValueType = std::decay_t<decltype(typedValue)>;
                    if constexpr (std::same_as<PropertyType, Int> && std::same_as<ValueType, int>) {
                        typedProperty.value = typedValue;
                    }
                    else if constexpr (std::same_as<PropertyType, Float> &&
                                       std::same_as<ValueType, float>) {
                        typedProperty.value = typedValue;
                    }
                    else if constexpr (std::same_as<PropertyType, Vec3> &&
                                       std::same_as<ValueType, glm::vec3>) {
                        typedProperty.value = typedValue;
                    }
                    else if constexpr (std::same_as<PropertyType, String> &&
                                       std::same_as<ValueType, std::string>) {
                        typedProperty.value = typedValue;
                    }
                    else {
                        CO_CORE_ASSERT(false, "Property type mismatch in update()");
                    }
                },
                value);
        },
        property);

    notifyChanged(handle);
}

PropertyHandle PropertySet::find(const std::string &name)
{
    const auto it = propertyLookup_.find(name);
    if (it == propertyLookup_.end()) {
        return {};
    }
    return it->second;
}

PropertyValue PropertySet::read(PropertyHandle handle)
{
    const auto &property = properties_[handle];
    return std::visit([](const auto &prop) -> PropertyValue { return prop.value; }, property);
}

PropertySet::PropertyProxy PropertySet::operator[](PropertyHandle handle)
{
    return PropertyProxy{.handle = handle, .set = this};
}

PropertySet::ChangedAwaiter PropertySet::changed(PropertyHandle handle)
{
    return ChangedAwaiter{*this, handle};
}

void PropertySet::registerWaiter(PropertyHandle handle, cppcoro::coroutine_handle<> awaiting)
{
    waiters_[handle].push_back(awaiting);
}

void PropertySet::unregisterWaiter(PropertyHandle handle, cppcoro::coroutine_handle<> awaiting)
{
    const auto it = waiters_.find(handle);
    if (it == waiters_.end()) {
        return;
    }

    auto &waiters = it->second;
    std::erase_if(waiters, [&](const auto &registered) {
        return registered.address() == awaiting.address();
    });
    if (waiters.empty()) {
        waiters_.erase(it);
    }
}

void PropertySet::notifyChanged(PropertyHandle handle)
{
    const auto it = waiters_.find(handle);
    if (it == waiters_.end()) {
        return;
    }

    auto awaitingCoroutines = std::move(it->second);
    waiters_.erase(it);

    for (const auto coroutine : awaitingCoroutines) {
        if (coroutine) {
            coroutine.resume();
        }
    }
}

} // namespace Cory::Prop

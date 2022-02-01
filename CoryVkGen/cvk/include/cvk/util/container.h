#pragma once

#include <algorithm>

namespace cvk::util {

template <typename ContainerType, typename ValueOrPredicateType>
bool contains(const ContainerType &container, ValueOrPredicateType &&value_or_predicate)
{
    if constexpr (std::is_same_v<typename ContainerType::value_type,
                                 std::remove_reference_t<ValueOrPredicateType>>) {
        auto it = std::find(std::begin(container), std::end(container), value_or_predicate);
        return it != std::end(container);
    }
    else {
        auto it = std::find_if(std::begin(container),
                               std::end(container),
                               std::forward<ValueOrPredicateType>(value_or_predicate));
        return it != std::end(container);
    }
}

template <typename ContainerType, typename ValueOrPredicateType>
typename ContainerType::value_type find_or(const ContainerType &container,
                                           ValueOrPredicateType &&value_or_predicate,
                                           typename ContainerType::value_type or_value)
{
    if constexpr (std::is_same_v<typename ContainerType::value_type,
                                 std::remove_reference_t<ValueOrPredicateType>>) {
        auto it = std::find(std::begin(container), std::end(container), value_or_predicate);
        return (it == std::end(container)) ? or_value : *it;
    }
    else {
        auto it = std::find_if(std::begin(container),
                               std::end(container),
                               std::forward<ValueOrPredicateType>(value_or_predicate));
        return (it == std::end(container)) ? or_value : *it;
    }
}

// template <typename ContainerType, typename PredicateFunctor>
// bool contains(const ContainerType &container, PredicateFunctor &&predicate)
// {
//     auto it = std::find_if(
//         std::begin(container), std::end(container), std::forward<PredicateFunctor>(predicate));
//     return it != std::end(container);
// }

} // namespace utils
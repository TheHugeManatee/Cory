#pragma once

#include <Cory/Base/Log.hpp>

#include <magic_enum/magic_enum.hpp>

#include <type_traits>

namespace Cory {

/**
 * EnumMap is a map-like container that stores a value of type U for each enumerator of the enum
 * type T.
 * @tparam T The enum type
 * @tparam U the value type
 *
 * It leverages the fact that enum types are known at compile time to provide more efficient
 * storage.
 *
 *  Usage example:
 *  enum class Color { Red, Green, Blue };
 *
 *  EnumMap<Color, std::string> colorNames;
 *  colorNames[Color::Red] = "Red";
 *  colorNames[Color::Green] = "Green";
 *  colorNames[Color::Blue] = "Blue";
 */
template <typename T, typename U>
    requires std::is_enum_v<T> && magic_enum::is_scoped_enum_v<T>
class EnumMap {
  public:
    /// Access the value for the given enum @a key
    U &operator[](T key)
    {
        const size_t index = static_cast<size_t>(key);
        CO_CORE_ASSERT(index < data_.size(), "EnumMap: key out of range");
        return data_[index];
    }
    /// Access the value for the given enum @a key
    const U &operator[](T key) const
    {
        const size_t index = static_cast<size_t>(key);
        CO_CORE_ASSERT(index < data_.size(), "EnumMap: key out of range");
        return data_[index];
    }

    /// Apply an operation on each stored value, ordered by enum value
    /// Func is getting passed the enum key and the value reference
    template <typename Func> void forEach(Func &&f)
    {
        for (std::size_t i = 0; i < data_.size(); ++i) {
            f(static_cast<T>(i), data_[i]);
        }
    }

  private:
    std::array<U, magic_enum::enum_count<T>()> data_{};
};
} // namespace Cory
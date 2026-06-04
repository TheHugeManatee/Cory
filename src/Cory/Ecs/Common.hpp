#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace Cory::Ecs {

struct Entity {
    uint32_t value{0};

    friend bool operator==(Entity, Entity) = default;
};

struct ResourceId {
    std::string name;

    ResourceId() = default;
    explicit ResourceId(std::string_view resourceName)
        : name(resourceName)
    {
    }

    friend bool operator==(const ResourceId &, const ResourceId &) = default;
    friend auto operator<=>(const ResourceId &, const ResourceId &) = default;
};

template <typename T> ResourceId resource()
{
    return ResourceId{typeid(std::remove_cvref_t<T>).name()};
}

inline ResourceId resource(std::string_view name)
{
    return ResourceId{name};
}

enum class Access {
    Read,
    Write,
};

struct ResourceAccess {
    ResourceId resource;
    Access access{Access::Read};

    friend bool operator==(const ResourceAccess &, const ResourceAccess &) = default;
};

inline ResourceAccess read(ResourceId id)
{
    return ResourceAccess{std::move(id), Access::Read};
}
inline ResourceAccess write(ResourceId id)
{
    return ResourceAccess{std::move(id), Access::Write};
}

template <typename T> ResourceAccess read()
{
    return read(resource<T>());
}

template <typename T> ResourceAccess write()
{
    return write(resource<T>());
}

using SystemFn = std::function<void()>;

} // namespace Cory::Ecs

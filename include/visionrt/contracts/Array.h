#pragma once

#include "visionrt/contracts/DataRef.h"
#include "visionrt/contracts/TypeSpec.h"

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace visionrt {

template<class T, std::size_t N>
struct NestedVectorType
{
    using type = std::vector<typename NestedVectorType<T, N - 1>::type>;
};

template<class T>
struct NestedVectorType<T, 0>
{
    using type = T;
};
template<class T, std::size_t N>
using NestedVector = typename NestedVectorType<T, N>::type;

namespace detail {

using ArrayScalars = std::tuple<std::uint8_t, std::int32_t, std::int64_t, float, double, bool, std::string>;

template<class T>
struct ArrayTraits
{
    using scalar_type = T;
};

template<class T>
struct ArrayTraits<std::vector<T>>
{
    using scalar_type = typename ArrayTraits<T>::scalar_type;
};

template<std::size_t Index>
using ArrayPayload = NestedVector<std::tuple_element_t<Index / kMaxArrayRank, ArrayScalars>, Index % kMaxArrayRank + 1>;

template<std::size_t... Indices>
auto arrayStorage(std::index_sequence<Indices...>) -> std::variant<DataRef<ArrayPayload<Indices>>...>;

using ArrayStorage
    = decltype(arrayStorage(std::make_index_sequence<std::tuple_size_v<ArrayScalars> * kMaxArrayRank>{}));

}

class Array
{
public:
    template<class T>
    requires std::is_constructible_v<detail::ArrayStorage, DataRef<T>> explicit Array(DataRef<T> value)
        : storage_(std::move(value))
    {
    }

    template<class T>
    requires std::is_constructible_v<detail::ArrayStorage, DataRef<T>> explicit Array(T                  value,
                                                                                      std::vector<Owner> keepAlive = {})
        : Array(DataRef<T>(std::move(value), std::move(keepAlive)))
    {
    }

    static Array empty(DType dtype, int rank)
    {
        const TypeSpec type{Type::Array, dtype, rank};
        validateTypeSpec(type);
        return Array(emptyStorage(static_cast<std::size_t>(dtype) * kMaxArrayRank + rank - 1));
    }

    static std::span<const TypeSpec> supportedTypes() noexcept
    {
        static constexpr auto types = []
        {
            std::array<TypeSpec, std::variant_size_v<detail::ArrayStorage>> result{};
            for (std::size_t index = 0; index < result.size(); ++index)
                result[index] = {Type::Array, static_cast<DType>(index / kMaxArrayRank),
                                 static_cast<int>(index % kMaxArrayRank + 1)};
            return result;
        }();
        return types;
    }

    TypeSpec type() const noexcept
    {
        return supportedTypes()[storage_.index()];
    }

    template<class T>
    T &get() const
    {
        return std::get<DataRef<T>>(storage_).get();
    }

    template<class Visitor>
    decltype(auto) visit(Visitor &&visitor) const
    {
        return std::visit(std::forward<Visitor>(visitor), storage_);
    }

    Array retainedBy(Owner dependency) const
    {
        return visit([&](const auto &value) { return Array(value.retainedBy(dependency)); });
    }

private:
    explicit Array(detail::ArrayStorage storage)
        : storage_(std::move(storage))
    {
    }

    template<std::size_t Index = 0>
    static detail::ArrayStorage emptyStorage(std::size_t selected)
    {
        if constexpr (Index < std::variant_size_v<detail::ArrayStorage>)
        {
            if (Index == selected)
                return detail::ArrayStorage(std::in_place_index<Index>, detail::ArrayPayload<Index>{});
            return emptyStorage<Index + 1>(selected);
        }
        else
        {
            throw ContractError("Unsupported Array dtype or rank");
        }
    }

    detail::ArrayStorage storage_;
};

}

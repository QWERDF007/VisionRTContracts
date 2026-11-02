#pragma once

#include "visionrt/contracts/DataRef.h"
#include "visionrt/contracts/TypeSpec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace visionrt {

// Readable nested type used by builder inputs and existing tool algorithms. It is not the Array
// storage: every Array owns one flat leaf buffer and describes its structure separately.
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

// The Array payload keeps one contiguous leaf buffer per dtype. Rank is described by the structure,
// never by the C++ storage type: a rectangular array carries a shape, an irregular array carries one
// offsets level per grouping. Both forms share the same typed buffer and the same accessors.
enum class ArrayLayout
{
    Dense,
    Ragged
};

namespace detail {

template<class T>
struct ArrayScalar
{
    using type = T;
};

template<class T>
struct ArrayScalar<std::vector<T>>
{
    using type = typename ArrayScalar<T>::type;
};

template<class T>
struct ArrayRank : std::integral_constant<int, 0>
{
};

template<class T>
struct ArrayRank<std::vector<T>> : std::integral_constant<int, ArrayRank<T>::value + 1>
{
};

template<class T>
constexpr DType arrayDType()
{
    using Scalar = typename ArrayScalar<std::remove_cvref_t<T>>::type;
    if constexpr (std::is_same_v<Scalar, std::uint8_t>)
        return DType::UInt8;
    else if constexpr (std::is_same_v<Scalar, std::int32_t>)
        return DType::Int32;
    else if constexpr (std::is_same_v<Scalar, std::int64_t>)
        return DType::Int64;
    else if constexpr (std::is_same_v<Scalar, float>)
        return DType::Float32;
    else if constexpr (std::is_same_v<Scalar, double>)
        return DType::Float64;
    else if constexpr (std::is_same_v<Scalar, bool>)
        return DType::Bool;
    else if constexpr (std::is_same_v<Scalar, std::string>)
        return DType::String;
    else
        static_assert(!sizeof(T), "不支持的数组元素类型");
}

template<DType>
struct ArrayScalarType;

template<>
struct ArrayScalarType<DType::UInt8>
{
    using type = std::uint8_t;
};

template<>
struct ArrayScalarType<DType::Int32>
{
    using type = std::int32_t;
};

template<>
struct ArrayScalarType<DType::Int64>
{
    using type = std::int64_t;
};

template<>
struct ArrayScalarType<DType::Float32>
{
    using type = float;
};

template<>
struct ArrayScalarType<DType::Float64>
{
    using type = double;
};

template<>
struct ArrayScalarType<DType::Bool>
{
    using type = std::uint8_t;
};

template<>
struct ArrayScalarType<DType::String>
{
    using type = std::string;
};

template<DType D>
using ArrayScalarTypeT = typename ArrayScalarType<D>::type;

template<class T>
using ArrayBufferT = std::conditional_t<std::is_same_v<typename ArrayScalar<std::remove_cvref_t<T>>::type, bool>,
                                        std::uint8_t,
                                        typename ArrayScalar<std::remove_cvref_t<T>>::type>;

inline std::size_t arrayElementCount(DType dtype)
{
    switch (dtype)
    {
    case DType::UInt8:
    case DType::Bool:
        return 1;
    case DType::Int32:
    case DType::Float32:
        return 4;
    case DType::Int64:
    case DType::Float64:
        return 8;
    case DType::String:
        return sizeof(std::string);
    }
    return 0;
}

} // namespace detail

class Array
{
public:
    Array()
        : storage_(denseTyped(DType::Float64, {0}, std::vector<double>{}, {}).storage_)
    {
    }

    // Empty value of the declared type. Rank 1 is a zero-length dense array; higher ranks keep a
    // zero in every dimension, which is the only shape an empty value can state without data.
    static Array empty(DType dtype, int rank)
    {
        if (rank < 1 || rank > kMaxArrayRank)
            throw ContractError("数组 rank 必须在支持范围内");
        std::vector<std::int64_t> shape(static_cast<std::size_t>(rank), 0);
        return dense(dtype, std::move(shape));
    }

    static Array dense(DType dtype, std::vector<std::int64_t> shape, std::vector<Owner> keepAlive = {})
    {
        switch (dtype)
        {
        case DType::UInt8:
            return denseTyped(dtype, std::move(shape), std::vector<std::uint8_t>{}, std::move(keepAlive));
        case DType::Int32:
            return denseTyped(dtype, std::move(shape), std::vector<std::int32_t>{}, std::move(keepAlive));
        case DType::Int64:
            return denseTyped(dtype, std::move(shape), std::vector<std::int64_t>{}, std::move(keepAlive));
        case DType::Float32:
            return denseTyped(dtype, std::move(shape), std::vector<float>{}, std::move(keepAlive));
        case DType::Float64:
            return denseTyped(dtype, std::move(shape), std::vector<double>{}, std::move(keepAlive));
        case DType::Bool:
            return denseTyped(dtype, std::move(shape), std::vector<std::uint8_t>{}, std::move(keepAlive));
        case DType::String:
            return denseTyped(dtype, std::move(shape), std::vector<std::string>{}, std::move(keepAlive));
        }
        throw ContractError("未知数组 dtype");
    }

    // std::vector<bool> is a proxy container; the payload always stores one byte per element.
    static Array dense(DType dtype, std::vector<std::int64_t> shape, std::vector<bool> values,
                       std::vector<Owner> keepAlive = {})
    {
        if (dtype != DType::Bool)
            throw ContractError("bool 值只能构造 Bool 数组");
        std::vector<std::uint8_t> bytes;
        bytes.reserve(values.size());
        for (const bool value : values) bytes.push_back(value ? 1 : 0);
        return denseTyped(DType::Bool, std::move(shape), std::move(bytes), std::move(keepAlive));
    }

    template<class T>
    static Array dense(DType dtype, std::vector<std::int64_t> shape, std::vector<T> values,
                       std::vector<Owner> keepAlive = {})
    {
        return denseTyped(dtype, std::move(shape), std::move(values), std::move(keepAlive));
    }

    // Borrows a contiguous buffer that somebody else owns (a NumPy view, for example). The owner is
    // kept alive by the Array and shared by every copy.
    static Array borrowedDense(DType dtype, std::vector<std::int64_t> shape, void *data, std::size_t count,
                               Owner owner)
    {
        validateDenseShape(dtype, shape, count);
        auto storage         = std::make_shared<Storage>();
        storage->dtype       = dtype;
        storage->layout      = ArrayLayout::Dense;
        storage->shape       = std::move(shape);
        storage->data        = data;
        storage->count       = count;
        storage->owner       = std::move(owner);
        return Array(std::move(storage));
    }

    static Array ragged(DType dtype, int rank, std::vector<std::vector<std::int64_t>> offsets,
                        std::vector<Owner> keepAlive = {})
    {
        switch (dtype)
        {
        case DType::UInt8:
            return raggedTyped(dtype, rank, std::move(offsets), std::vector<std::uint8_t>{}, std::move(keepAlive));
        case DType::Int32:
            return raggedTyped(dtype, rank, std::move(offsets), std::vector<std::int32_t>{}, std::move(keepAlive));
        case DType::Int64:
            return raggedTyped(dtype, rank, std::move(offsets), std::vector<std::int64_t>{}, std::move(keepAlive));
        case DType::Float32:
            return raggedTyped(dtype, rank, std::move(offsets), std::vector<float>{}, std::move(keepAlive));
        case DType::Float64:
            return raggedTyped(dtype, rank, std::move(offsets), std::vector<double>{}, std::move(keepAlive));
        case DType::Bool:
            return raggedTyped(dtype, rank, std::move(offsets), std::vector<std::uint8_t>{}, std::move(keepAlive));
        case DType::String:
            return raggedTyped(dtype, rank, std::move(offsets), std::vector<std::string>{}, std::move(keepAlive));
        }
        throw ContractError("未知数组 dtype");
    }

    static Array ragged(DType dtype, int rank, std::vector<std::vector<std::int64_t>> offsets,
                        std::vector<bool> values, std::vector<Owner> keepAlive = {})
    {
        if (dtype != DType::Bool)
            throw ContractError("bool 值只能构造 Bool 数组");
        std::vector<std::uint8_t> bytes;
        bytes.reserve(values.size());
        for (const bool value : values) bytes.push_back(value ? 1 : 0);
        return raggedTyped(DType::Bool, rank, std::move(offsets), std::move(bytes), std::move(keepAlive));
    }

    template<class T>
    static Array ragged(DType dtype, int rank, std::vector<std::vector<std::int64_t>> offsets,
                        std::vector<T> values, std::vector<Owner> keepAlive = {})
    {
        return raggedTyped(dtype, rank, std::move(offsets), std::move(values), std::move(keepAlive));
    }

    // Builder input in the readable nested form. A rectangular tree becomes a Dense array; a tree
    // whose levels have differing lengths becomes Ragged with one offsets level per grouping.
    template<class T>
    static Array fromNested(T nested)
    {
        constexpr int rank = detail::ArrayRank<std::remove_cvref_t<T>>::value;
        static_assert(rank >= 1, "Array::fromNested 需要至少一层的 vector");
        const DType dtype = detail::arrayDType<T>();
        if constexpr (rank == 1)
        {
            std::vector<std::int64_t> shape{static_cast<std::int64_t>(nested.size())};
            return dense(dtype, std::move(shape), flatten<T>(nested));
        }
        else
        {
            std::vector<std::int64_t> shape;
            shape.reserve(rank);
            if (isRectangular(nested, 0, shape, rank))
            {
                shape.resize(static_cast<std::size_t>(rank), 0);
                auto values = flatten<T>(nested);
                return dense(dtype, std::move(shape), std::move(values));
            }
            std::vector<std::vector<std::int64_t>> offsets(static_cast<std::size_t>(rank - 1));
            for (auto &level : offsets) level.push_back(0);
            auto values = flattenRagged<T>(nested, 0, offsets);
            return ragged(dtype, rank, std::move(offsets), std::move(values));
        }
    }

    // Deep copy: a project literal must not share configuration with the node it came from.
    Array clone() const
    {
        Storage copy;
        copy.dtype  = storage_->dtype;
        copy.layout = storage_->layout;
        copy.shape  = storage_->shape;
        copy.offsets = storage_->offsets;
        copy.count  = storage_->count;
        copy.keepAlive = storage_->keepAlive;
        copy.owner  = cloneBuffer();
        copy.data   = ownerData(copy.owner, copy.dtype);
        return Array(std::make_shared<Storage>(std::move(copy)));
    }

    Array retainedBy(Owner dependency) const
    {
        Storage copy    = *storage_;
        copy.keepAlive.push_back(std::move(dependency));
        return Array(std::make_shared<Storage>(std::move(copy)));
    }

    static std::span<const TypeSpec> supportedTypes() noexcept
    {
        static constexpr auto types = []
        {
            std::array<TypeSpec, 7 * kMaxArrayRank> result{};
            for (std::size_t index = 0; index < result.size(); ++index)
                result[index] = {Type::Array, static_cast<DType>(index / kMaxArrayRank),
                                 static_cast<int>(index % kMaxArrayRank + 1)};
            return result;
        }();
        return types;
    }

    TypeSpec type() const noexcept
    {
        return TypeSpec{Type::Array, storage_->dtype, rank()};
    }

    DType                     dtype() const noexcept { return storage_->dtype; }
    ArrayLayout               layout() const noexcept { return storage_->layout; }
    int                       rank() const noexcept
    {
        return storage_->layout == ArrayLayout::Dense ? static_cast<int>(storage_->shape.size())
                                                      : static_cast<int>(storage_->offsets.size()) + 1;
    }
    std::size_t               elementCount() const noexcept { return storage_->count; }
    void                     *data() const noexcept { return storage_->data; }
    Owner                     bufferOwner() const { return storage_->owner; }
    const std::vector<std::int64_t>              &shape() const noexcept { return storage_->shape; }
    const std::vector<std::vector<std::int64_t>> &offsets() const noexcept { return storage_->offsets; }

    // An Array is a handle to shared storage, like DataRef: copies see the same elements and an
    // existing element stays writable through any of them. Structure changes (shape, ragged
    // groupings) go through a builder and a new Array, never through resize/push_back here.
    template<class T>
    std::span<T> elements() const
    {
        if (detail::arrayDType<T>() != storage_->dtype)
            throw ContractError("数组元素类型与 dtype 不一致");
        return {static_cast<T *>(storage_->data), storage_->count};
    }

    // Bool is stored one byte per element and never through std::vector<bool>.
    std::span<std::uint8_t> boolBytes() const
    {
        if (storage_->dtype != DType::Bool)
            throw ContractError("数组不是 Bool");
        return {static_cast<std::uint8_t *>(storage_->data), storage_->count};
    }

    bool operator==(const Array &other) const
    {
        if (storage_.get() == other.storage_.get())
            return true;
        if (type() != other.type() || storage_->layout != other.storage_->layout
            || storage_->shape != other.storage_->shape || storage_->offsets != other.storage_->offsets
            || storage_->count != other.storage_->count)
            return false;
        return equalBuffers(other);
    }

    bool operator!=(const Array &other) const { return !(*this == other); }

private:
    struct Storage
    {
        // Declaration order is destruction order reversed: keepAlive is released last so a value
        // that pins a module (an owner created by a plugin DLL) still has that module loaded while
        // its buffer owner and structural vectors are destroyed.
        std::vector<Owner>                    keepAlive;
        DType                                 dtype  = DType::Float64;
        ArrayLayout                           layout = ArrayLayout::Dense;
        std::vector<std::int64_t>             shape;
        std::vector<std::vector<std::int64_t>> offsets;
        void                                 *data  = nullptr;
        std::size_t                           count = 0;
        std::shared_ptr<void>                 owner;
    };

    explicit Array(std::shared_ptr<Storage> storage)
        : storage_(std::move(storage))
    {
    }

    template<class T>
    static Array denseTyped(DType dtype, std::vector<std::int64_t> shape, std::vector<T> values,
                            std::vector<Owner> keepAlive)
    {
        const DType valueType = detail::arrayDType<T>();
        if (valueType != dtype && !(dtype == DType::Bool && valueType == DType::UInt8))
            throw ContractError("数组值类型与 dtype 不一致");
        validateDenseShape(dtype, shape, values.size());
        auto buffer  = std::make_shared<std::vector<T>>(std::move(values));
        auto storage = std::make_shared<Storage>();
        storage->dtype   = dtype;
        storage->layout  = ArrayLayout::Dense;
        storage->shape   = std::move(shape);
        storage->data    = buffer->data();
        storage->count   = buffer->size();
        storage->owner   = std::move(buffer);
        storage->keepAlive = std::move(keepAlive);
        return Array(std::move(storage));
    }

    template<class T>
    static Array raggedTyped(DType dtype, int rank, std::vector<std::vector<std::int64_t>> offsets,
                             std::vector<T> values, std::vector<Owner> keepAlive)
    {
        const DType valueType = detail::arrayDType<T>();
        if (valueType != dtype && !(dtype == DType::Bool && valueType == DType::UInt8))
            throw ContractError("数组值类型与 dtype 不一致");
        validateRaggedShape(rank, offsets, values.size());
        auto buffer  = std::make_shared<std::vector<T>>(std::move(values));
        auto storage = std::make_shared<Storage>();
        storage->dtype   = dtype;
        storage->layout  = ArrayLayout::Ragged;
        storage->offsets = std::move(offsets);
        storage->data    = buffer->data();
        storage->count   = buffer->size();
        storage->owner   = std::move(buffer);
        storage->keepAlive = std::move(keepAlive);
        return Array(std::move(storage));
    }

    static void validateDenseShape(DType dtype, const std::vector<std::int64_t> &shape, std::size_t count)
    {
        if (shape.empty() || shape.size() > static_cast<std::size_t>(kMaxArrayRank))
            throw ContractError("数组 rank 必须在支持范围内");
        std::size_t expected = 1;
        for (const auto dimension : shape)
        {
            if (dimension < 0)
                throw ContractError("数组维度不能为负");
            if (dimension == 0)
            {
                expected = 0;
                continue;
            }
            if (expected != 0 && static_cast<std::size_t>(dimension) > std::numeric_limits<std::size_t>::max() / expected)
                throw ContractError("数组元素数量溢出");
            expected *= static_cast<std::size_t>(dimension);
        }
        if (expected != count)
            throw ContractError("数组 shape 与元素数量不一致");
        (void)dtype;
    }

    static void validateRaggedShape(int rank, const std::vector<std::vector<std::int64_t>> &offsets,
                                    std::size_t count)
    {
        if (rank < 2 || rank > kMaxArrayRank)
            throw ContractError("Ragged 数组 rank 必须在 2..4");
        if (offsets.size() != static_cast<std::size_t>(rank - 1))
            throw ContractError("Ragged 数组的 offsets 层数与 rank 不一致");
        for (std::size_t level = 0; level < offsets.size(); ++level)
        {
            const auto &values = offsets[level];
            if (values.empty() || values.front() != 0)
                throw ContractError("Ragged offsets 必须从 0 开始");
            for (std::size_t index = 1; index < values.size(); ++index)
                if (values[index] < values[index - 1])
                    throw ContractError("Ragged offsets 必须单调不减");
            if (level + 1 < offsets.size())
            {
                if (values.back() != static_cast<std::int64_t>(offsets[level + 1].size()) - 1)
                    throw ContractError("Ragged offsets 的末项必须对应下一层的分组数");
            }
            else if (values.back() != static_cast<std::int64_t>(count))
                throw ContractError("Ragged offsets 的末项必须对应叶子数量");
        }
    }

    static void *ownerData(const std::shared_ptr<void> &owner, DType dtype)
    {
        switch (dtype)
        {
        case DType::UInt8:
            return static_cast<std::vector<std::uint8_t> *>(owner.get())->data();
        case DType::Int32:
            return static_cast<std::vector<std::int32_t> *>(owner.get())->data();
        case DType::Int64:
            return static_cast<std::vector<std::int64_t> *>(owner.get())->data();
        case DType::Float32:
            return static_cast<std::vector<float> *>(owner.get())->data();
        case DType::Float64:
            return static_cast<std::vector<double> *>(owner.get())->data();
        case DType::Bool:
            return static_cast<std::vector<std::uint8_t> *>(owner.get())->data();
        case DType::String:
            return static_cast<std::vector<std::string> *>(owner.get())->data();
        }
        return nullptr;
    }

    template<class T>
    std::shared_ptr<void> copyBuffer() const
    {
        auto values = std::make_shared<std::vector<T>>(static_cast<const T *>(storage_->data),
                                                       static_cast<const T *>(storage_->data) + storage_->count);
        return values;
    }

    std::shared_ptr<void> cloneBuffer() const
    {
        switch (storage_->dtype)
        {
        case DType::UInt8:
        case DType::Bool:
            return copyBuffer<std::uint8_t>();
        case DType::Int32:
            return copyBuffer<std::int32_t>();
        case DType::Int64:
            return copyBuffer<std::int64_t>();
        case DType::Float32:
            return copyBuffer<float>();
        case DType::Float64:
            return copyBuffer<double>();
        case DType::String:
            return copyBuffer<std::string>();
        }
        throw ContractError("未知数组 dtype");
    }

    template<class T>
    bool equalBuffer(const Array &other) const
    {
        return std::equal(static_cast<const T *>(storage_->data),
                          static_cast<const T *>(storage_->data) + storage_->count,
                          static_cast<const T *>(other.storage_->data));
    }

    bool equalBuffers(const Array &other) const
    {
        switch (storage_->dtype)
        {
        case DType::UInt8:
        case DType::Bool:
            return equalBuffer<std::uint8_t>(other);
        case DType::Int32:
            return equalBuffer<std::int32_t>(other);
        case DType::Int64:
            return equalBuffer<std::int64_t>(other);
        case DType::Float32:
            return equalBuffer<float>(other);
        case DType::Float64:
            return equalBuffer<double>(other);
        case DType::String:
            return equalBuffer<std::string>(other);
        }
        return false;
    }

    template<class T>
    static bool isRectangular(const T &node, int level, std::vector<std::int64_t> &shape, int rank)
    {
        if (level >= rank)
            return true;
        if (shape.size() <= static_cast<std::size_t>(level))
            shape.push_back(static_cast<std::int64_t>(node.size()));
        else if (shape[static_cast<std::size_t>(level)] != static_cast<std::int64_t>(node.size()))
            return false;
        if constexpr (detail::ArrayRank<std::remove_cvref_t<T>>::value > 1)
            for (const auto &child : node)
                if (!isRectangular(child, level + 1, shape, rank))
                    return false;
        return true;
    }

    template<class T>
    static std::vector<detail::ArrayBufferT<T>> flatten(T &node)
    {
        std::vector<detail::ArrayBufferT<T>> values;
        flattenInto(node, values);
        return values;
    }

    template<class T>
    static void flattenInto(T &node, std::vector<detail::ArrayBufferT<T>> &values)
    {
        if constexpr (detail::ArrayRank<std::remove_cvref_t<T>>::value > 1)
        {
            for (auto &child : node) flattenInto(child, values);
        }
        else
        {
            using Scalar = typename detail::ArrayScalar<std::remove_cvref_t<T>>::type;
            for (auto &&element : node)
            {
                if constexpr (std::is_same_v<Scalar, bool>)
                    values.push_back(static_cast<bool>(element) ? 1 : 0);
                else
                    values.push_back(std::move(element));
            }
        }
    }

    template<class T>
    static std::vector<detail::ArrayBufferT<T>> flattenRagged(
        T &node, int level, std::vector<std::vector<std::int64_t>> &offsets)
    {
        std::vector<detail::ArrayBufferT<T>> values;
        if constexpr (detail::ArrayRank<std::remove_cvref_t<T>>::value > 2)
        {
            for (auto &child : node)
            {
                auto nested = flattenRagged(child, level + 1, offsets);
                values.insert(values.end(), std::make_move_iterator(nested.begin()),
                              std::make_move_iterator(nested.end()));
                offsets[static_cast<std::size_t>(level)].push_back(
                    offsets[static_cast<std::size_t>(level)].back() + static_cast<std::int64_t>(child.size()));
            }
        }
        else
        {
            for (auto &child : node)
            {
                offsets[static_cast<std::size_t>(level)].push_back(
                    offsets[static_cast<std::size_t>(level)].back() + static_cast<std::int64_t>(child.size()));
                flattenInto(child, values);
            }
        }
        return values;
    }

    std::shared_ptr<Storage> storage_;
};

namespace detail {

template<class Scalar>
Scalar nestedLeaf(const Array &array, std::size_t index)
{
    if constexpr (std::is_same_v<Scalar, bool>)
        return array.boolBytes()[index] != 0;
    else
        return array.elements<Scalar>()[index];
}

// The two layouts differ only in where a group's children come from: a Dense level repeats the
// stored shape, a Ragged level reads the one offsets level that groups that depth.
template<class Nested>
void fillNested(const Array &array, int level, std::size_t group, std::size_t &leaf, Nested &out)
{
    using Node          = std::remove_cvref_t<Nested>;
    using Scalar        = typename ArrayScalar<Node>::type;
    constexpr int rank  = ArrayRank<Node>::value;
    const bool    dense = array.layout() == ArrayLayout::Dense;

    std::size_t first = 0;
    std::size_t count = 0;
    if (dense)
    {
        count = static_cast<std::size_t>(array.shape()[static_cast<std::size_t>(level)]);
    }
    else if constexpr (rank == 1)
    {
        // The leaf vector at this depth is grouped by the last offsets level.
        const auto &bounds = array.offsets()[static_cast<std::size_t>(level) - 1];
        first              = static_cast<std::size_t>(bounds[group]);
        count              = static_cast<std::size_t>(bounds[group + 1] - bounds[group]);
    }
    else if (level == 0)
    {
        // The top level holds one root group; its children are the whole first offsets level.
        count = array.offsets().front().size() - 1;
    }
    else
    {
        const auto &bounds = array.offsets()[static_cast<std::size_t>(level) - 1];
        first              = static_cast<std::size_t>(bounds[group]);
        count              = static_cast<std::size_t>(bounds[group + 1] - bounds[group]);
    }
    if constexpr (rank == 1)
    {
        for (std::size_t index = 0; index < count; ++index) out.push_back(nestedLeaf<Scalar>(array, leaf++));
    }
    else
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            typename Node::value_type child;
            fillNested(array, level + 1, dense ? 0 : first + index, leaf, child);
            out.push_back(std::move(child));
        }
    }
}

} // namespace detail

// Materialises the readable nested form a declaration, a document or a Python author uses. It is
// the inverse of fromNested and exists for consumers that print, compare or export a value; the
// Array never stores the nested tree. Throws when the requested tree does not describe this Array.
template<class Nested>
Nested nestedOf(const Array &array)
{
    using Node = std::remove_cvref_t<Nested>;
    if (array.rank() != detail::ArrayRank<Node>::value || array.dtype() != detail::arrayDType<Node>())
        throw ContractError("nestedOf：嵌套类型与数组结构不一致");
    Nested      result;
    std::size_t leaf = 0;
    detail::fillNested(array, 0, 0, leaf, result);
    return result;
}

} // namespace visionrt

#include <visionrt/contracts/Array.h>
#include <visionrt/contracts/Value.h>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>

using namespace visionrt;

namespace {
template<class Element>
void arrays(DType dtype, Element first, Element second)
{
    const Array rank1(NestedVector<Element, 1>{first, second});
    const Array rank2(NestedVector<Element, 2>{
        {first},
        {},
        {second, first}
    });
    const Array rank3(NestedVector<Element, 3>{
        {{first}, {}},
        {},
        {{second, first}}
    });
    const Array rank4(NestedVector<Element, 4>{
        {{{first}, {}}, {}},
        {},
        {{{second, first}}}
    });
    int         rank = 0;
    for (const auto &value : {rank1, rank2, rank3, rank4})
    {
        const TypeSpec expected{Type::Array, dtype, ++rank};
        assert(value.type() == expected);
        validateValue(expected, value);
        assert(!equalValues(value, Array::empty(dtype, rank)));
    }
    using Payload                    = NestedVector<Element, 4>;
    const auto alias                 = rank4;
    alias.get<Payload>()[0][0][0][0] = second;
    assert(rank4.get<Payload>()[0][0][0][0] == second);
    assert(&rank4.get<Payload>() == &alias.get<Payload>());
}
}

int main()
try
{
    static_assert(std::is_same_v<NestedVector<std::string, 5>,
                                 std::vector<std::vector<std::vector<std::vector<std::vector<std::string>>>>>>);
    using Integers = NestedVector<std::int64_t, 2>;
    const Array values(Integers{
        {9007199254740993LL},
        {},
        {7, 8}
    });
    const auto  alias = values;
    alias.get<Integers>()[2].push_back(9);
    assert(&values.get<Integers>() == &alias.get<Integers>());
    assert(values.get<Integers>()[0][0] == 9007199254740993LL);
    assert(values.get<Integers>()[1].empty());
    assert(values.get<Integers>()[2].back() == 9);
    assert((values.type() == TypeSpec{Type::Array, DType::Int64, 2}));
    arrays<std::uint8_t>(DType::UInt8, 0, 255);
    arrays<std::int32_t>(DType::Int32, -2147483647 - 1, 2147483647);
    arrays<std::int64_t>(DType::Int64, -9223372036854775807LL - 1, 9007199254740993LL);
    arrays<float>(DType::Float32, 1.25F, -2.5F);
    arrays<double>(DType::Float64, 16777217.125, -2.5);
    arrays<bool>(DType::Bool, true, false);
    arrays<std::string>(DType::String, "中文 🌍", "");
    assert(Array::supportedTypes().size() == 28);
    int released = 0;
    for (const auto &type : Array::supportedTypes())
    {
        std::weak_ptr<void> weak;
        PortValue           retained;
        {
            Owner dependency(new int(7),
                             [&](void *token)
                             {
                                 delete static_cast<int *>(token);
                                 ++released;
                             });
            weak              = dependency;
            const auto source = Array::empty(*type.dtype, type.rank);
            retained          = retainValue(source, dependency);
            assert(typeOf(retained) == type);
            assert(equalValues(source, retained));
        }
        assert(!weak.expired());
        retained = Missing{};
        assert(weak.expired());
    }
    assert(released == 28);
    std::cout << "Typed ragged arrays share writable vectors\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}

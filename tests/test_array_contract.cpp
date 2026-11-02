#include <visionrt/contracts/Array.h>
#include <visionrt/contracts/Value.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace visionrt;

namespace {
// elements<T> reads the flat scalar buffer: a nested vector describes structure and bool is
// byte-backed through boolBytes(), so both are compile-time errors instead of a reinterpret_cast
// over a buffer of a different element width.
template<class T>
concept ReadsElements = requires(const Array &array) { array.template elements<T>(); };

static_assert(ReadsElements<std::uint8_t>);
static_assert(ReadsElements<std::int32_t>);
static_assert(ReadsElements<std::int64_t>);
static_assert(ReadsElements<float>);
static_assert(ReadsElements<double>);
static_assert(ReadsElements<std::string>);
static_assert(!ReadsElements<bool>);
static_assert(!ReadsElements<std::vector<double>>);
static_assert(!ReadsElements<double *>);

template<class T>
void rectangular(DType dtype)
{
    for (int rank = 1; rank <= kMaxArrayRank; ++rank)
    {
        std::vector<std::int64_t> shape(static_cast<std::size_t>(rank), 2);
        std::size_t               count = 1;
        for (int level = 0; level < rank; ++level) count *= 2;
        std::vector<T> values(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            if constexpr (std::is_same_v<T, std::string>)
                values[index] = std::to_string(index);
            else if constexpr (std::is_same_v<T, bool>)
                values[index] = index % 2 == 0;
            else
                values[index] = static_cast<T>(index + 1);
        }
        const auto array = Array::dense(dtype, shape, values);
        assert(array.type() == (TypeSpec{Type::Array, dtype, rank}));
        assert(array.layout() == ArrayLayout::Dense);
        assert(array.rank() == rank);
        assert(array.shape() == shape);
        assert(array.elementCount() == count);
        if constexpr (std::is_same_v<T, bool>)
        {
            const auto bytes = array.boolBytes();
            assert(bytes.size() == count);
            for (std::size_t index = 0; index < count; ++index)
                assert((bytes[index] != 0) == values[index]);
        }
        else
        {
            const auto span = array.elements<T>();
            assert(span.size() == count);
            for (std::size_t index = 0; index < count; ++index) assert(span[index] == values[index]);
        }
        validateValue(array.type(), array);
        assert(!equalValues(array, Array::empty(dtype, rank)));
    }
}

void raggedStructures()
{
    // rank 2: three rows of lengths 1, 0, 2.
    const auto rows = Array::fromNested(std::vector<std::vector<std::int64_t>>{{7}, {}, {8, 9}});
    assert(rows.type() == (TypeSpec{Type::Array, DType::Int64, 2}));
    assert(rows.layout() == ArrayLayout::Ragged);
    assert((rows.offsets() == std::vector<std::vector<std::int64_t>>{{0, 1, 1, 3}}));
    const std::vector<std::int64_t> flat{7, 8, 9};
    assert(std::equal(flat.begin(), flat.end(), rows.elements<std::int64_t>().begin()));

    // rank 3: two matrices, one empty, one with two rows of lengths 1 and 2.
    const auto matrices
        = Array::fromNested(std::vector<std::vector<std::vector<double>>>{{}, {{1.0}, {2.0, 3.0}}});
    assert(matrices.type() == (TypeSpec{Type::Array, DType::Float64, 3}));
    assert(matrices.layout() == ArrayLayout::Ragged);
    assert((matrices.offsets() == std::vector<std::vector<std::int64_t>>({{0, 0, 2}, {0, 1, 3}})));
    const std::vector<double> matrixValues{1.0, 2.0, 3.0};
    assert(std::equal(matrixValues.begin(), matrixValues.end(), matrices.elements<double>().begin()));

    // rank 4: one rectangular path stays dense.
    const auto deep = Array::fromNested(
        std::vector<std::vector<std::vector<std::vector<float>>>>{{{{{1.0F, 2.0F, 3.0F}}}}});
    assert(deep.type() == (TypeSpec{Type::Array, DType::Float32, 4}));
    assert(deep.layout() == ArrayLayout::Dense);
    assert((deep.shape() == std::vector<std::int64_t>{1, 1, 1, 3}));

    // rank 4 ragged: group 0 has one matrix, group 1 has an empty and a non-empty matrix.
    std::vector<std::vector<std::vector<std::vector<float>>>> raggedTree{
        {{{{1.0F, 2.0F, 3.0F}}}},
        {{}, {{4.0F}}}
    };
    const auto ragged4 = Array::fromNested(std::move(raggedTree));
    assert(ragged4.layout() == ArrayLayout::Ragged);
    assert(ragged4.offsets().size() == 3);
    assert((ragged4.offsets()[0] == std::vector<std::int64_t>{0, 1, 3}));
    assert((ragged4.offsets()[1] == std::vector<std::int64_t>{0, 1, 1, 2}));
    assert((ragged4.offsets()[2] == std::vector<std::int64_t>{0, 3, 4}));

    // A rectangular nested tree stays dense even when it is empty.
    const auto emptyRows = Array::fromNested(std::vector<std::vector<std::string>>{});
    assert(emptyRows.layout() == ArrayLayout::Dense);
    assert((emptyRows.shape() == std::vector<std::int64_t>{0, 0}));
    assert(emptyRows.elementCount() == 0);

    // Explicit structure preserves a zero-width rectangle.
    const auto zeroWidth = Array::dense(DType::Float64, {0, 3});
    assert((zeroWidth.shape() == std::vector<std::int64_t>{0, 3}));
    assert(zeroWidth.elementCount() == 0);

    // Ragged with zero groups keeps the declared rank and dtype.
    const auto noGroups = Array::ragged(DType::Bool, 2, {{0}});
    assert(noGroups.type() == (TypeSpec{Type::Array, DType::Bool, 2}));
    assert(noGroups.layout() == ArrayLayout::Ragged);
    assert(noGroups.elementCount() == 0);
}

void sharingAndReplacement()
{
    auto values = Array::dense(DType::Float64, {2, 2}, std::vector<double>{1.0, 2.0, 3.0, 4.0});
    auto alias  = values;
    alias.elements<double>()[0] = 9.0;
    assert(values.elements<double>()[0] == 9.0);

    const auto isolated = values.clone();
    values.elements<double>()[1] = 8.0;
    assert(isolated.elements<double>()[1] == 2.0);

    // Replacing the structure is a new value; the published buffer is never resized.
    const auto reshaped = Array::dense(DType::Float64, {4}, std::vector<double>{1.0, 2.0, 3.0, 4.0});
    assert(values.elementCount() == 4);
    assert((reshaped.shape() == std::vector<std::int64_t>{4}));
    assert(reshaped.layout() == ArrayLayout::Dense);

    // A borrowed buffer shares its owner but not its bytes.
    auto       owner = std::make_shared<std::vector<float>>(std::vector<float>{1.0F, 2.0F});
    const auto borrowed = Array::borrowedDense(DType::Float32, {2}, owner->data(), owner->size(), owner);
    assert(borrowed.elements<float>()[0] == 1.0F);
    (*owner)[0] = 5.0F;
    assert(borrowed.elements<float>()[0] == 5.0F);
    owner.reset();
    assert(borrowed.elements<float>()[1] == 2.0F); // the Array keeps the buffer alive
}

void ownership()
{
    int                 released = 0;
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
        const auto source = Array::empty(DType::Float64, 3);
        retained          = retainValue(source, dependency);
        assert(typeOf(retained) == (TypeSpec{Type::Array, DType::Float64, 3}));
        assert(equalValues(source, retained));
    }
    assert(!weak.expired());
    retained = Missing{};
    assert(weak.expired());
    assert(released == 1);
}
} // namespace

namespace {

// The readable nested tree is the builder input and the consumer view; it is never the storage.
void nestedRoundTrip()
{
    const auto dense = Array::fromNested(std::vector<std::vector<double>>{{1.0, 2.0}, {3.0, 4.0}});
    assert((nestedOf<std::vector<std::vector<double>>>(dense)
            == std::vector<std::vector<double>>{{1.0, 2.0}, {3.0, 4.0}}));

    const auto rows = Array::fromNested(std::vector<std::vector<std::int64_t>>{{7}, {}, {8, 9}});
    assert((nestedOf<std::vector<std::vector<std::int64_t>>>(rows)
            == std::vector<std::vector<std::int64_t>>{{7}, {}, {8, 9}}));

    const auto matrices = Array::fromNested(std::vector<std::vector<std::vector<double>>>{{}, {{1.0}, {2.0, 3.0}}});
    assert((nestedOf<std::vector<std::vector<std::vector<double>>>>(matrices)
            == std::vector<std::vector<std::vector<double>>>{{}, {{1.0}, {2.0, 3.0}}}));

    const std::vector<std::vector<std::vector<std::vector<float>>>> tree{{{{{1.0F, 2.0F}}}}, {{}, {{3.0F}}}};
    const auto                                                       deep = Array::fromNested(tree);
    assert(deep.layout() == ArrayLayout::Ragged);
    assert(nestedOf<std::vector<std::vector<std::vector<std::vector<float>>>>>(deep) == tree);

    // Bool keeps its own semantics in both directions instead of becoming UInt8.
    const auto flags = Array::fromNested(std::vector<std::vector<bool>>{{true, false}, {}});
    assert((nestedOf<std::vector<std::vector<bool>>>(flags) == std::vector<std::vector<bool>>{{true, false}, {}}));

    // An empty higher-rank value keeps the zero shape it was declared with.
    const auto empty = Array::empty(DType::Float64, 3);
    assert(nestedOf<std::vector<std::vector<std::vector<double>>>>(empty).empty());

    // A tree that does not describe the stored dtype/rank is rejected, never reinterpreted.
    bool rejected = false;
    try
    {
        (void)nestedOf<std::vector<std::vector<double>>>(rows);
    }
    catch (const ContractError &)
    {
        rejected = true;
    }
    assert(rejected);
}

// One token per declaration type; the token is the only spelling an author writes.
void typeTokens()
{
    const std::array<TypeSpec, 11> types{{
        {Type::Int},
        {Type::Float},
        {Type::Bool},
        {Type::String},
        {Type::Image},
        {Type::ROI},
        {Type::Array, DType::Int64, 1},
        {Type::Array, DType::Int64, 2},
        {Type::Array, DType::Float32, 2},
        {Type::Array, DType::Float64, 3},
        {Type::Array, DType::String, 4},
    }};
    for (const auto &type : types) assert(parseTypeToken(typeToken(type)) == type);
    assert(typeToken({Type::Array, DType::Int64, 2}) == "vec_vec_int");
    assert(typeToken({Type::Array, DType::Float32, 2}) == "vec_vec_float32");
    assert(typeToken({Type::Array, DType::Float64, 3}) == "vec_vec_vec_float");
    assert(parseTypeToken("vec_int") == (TypeSpec{Type::Array, DType::Int64, 1}));

    bool rejected = false;
    for (const auto *token : {"int32", "float32", "uint8", "double", "int64", "vec_vec_vec_vec_vec_int"})
    {
        try
        {
            (void)parseTypeToken(token);
        }
        catch (const ContractError &)
        {
            rejected = true;
        }
        assert(rejected);
        rejected = false;
    }
}

// Every declared port always has a value; the type supplies it when the author writes none.
void typeDefaults()
{
    assert(std::get<std::int64_t>(defaultValue({Type::Int})) == 0);
    assert(std::get<double>(defaultValue({Type::Float})) == 0.0);
    assert(std::get<bool>(defaultValue({Type::Bool})) == false);
    assert(std::get<std::string>(defaultValue({Type::String})).empty());
    assert(std::get<DataRef<Image>>(defaultValue({Type::Image})).get().empty());
    assert(std::get<DataRef<ROI>>(defaultValue({Type::ROI})).get().empty());

    const auto emptyArray = std::get<Array>(defaultValue({Type::Array, DType::Int64, 2}));
    assert(emptyArray.type() == (TypeSpec{Type::Array, DType::Int64, 2}));
    assert(emptyArray.elementCount() == 0);
    assert((emptyArray.shape() == std::vector<std::int64_t>{0, 0}));
}

} // namespace

int main()
try
{
    rectangular<std::uint8_t>(DType::UInt8);    rectangular<std::int32_t>(DType::Int32);
    rectangular<std::int64_t>(DType::Int64);
    rectangular<float>(DType::Float32);
    rectangular<double>(DType::Float64);
    rectangular<bool>(DType::Bool);
    rectangular<std::string>(DType::String);

    raggedStructures();
    sharingAndReplacement();
    ownership();
    nestedRoundTrip();
    typeTokens();
    typeDefaults();

    std::cout << "Array contract passed: flat dtype buffers, Dense/Ragged structure, shared writable "
                 "elements, clone isolation and retained owners\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}

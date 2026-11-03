#include <visionrt/contracts/FunctionTool.h>
#include <visionrt/contracts/Parameters.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace visionrt;

namespace {
struct TestRegistry final : IToolRegistry
{
    std::vector<ToolFactoryDescriptor> descriptors;

    ToolStatus add(const ToolFactoryDescriptor &descriptor) noexcept override
    {
        for (const auto &existing : descriptors)
            if (std::string_view(existing.tool_id) == descriptor.tool_id)
                return {-3, "工具工厂 ID 重复"};
        descriptors.push_back(descriptor);
        return {};
    }
};

ToolResult addOne(const Values &inputs, const ToolServices &)
{
    InputReader reader(inputs);
    const auto  value = reader.integer(1);
    if (!value)
        return toolResult({{2, Missing{}}}, {1001, "输入缺失", value.issue().describe()});
    return toolResult({{2, value.value() + 1}}, executionSuccess());
}

ToolResult failLoudly(const Values &, const ToolServices &)
{
    throw ContractError("函数内业务失败");
}

struct Counter final : IToolInstance
{
    explicit Counter(ToolServices)
    {
    }

    ToolStatus initialize(const Values &) override
    {
        return {};
    }

    ToolResult execute(const Values &) override
    {
        return toolResult({{2, ++calls}}, {0, ""});
    }

    ToolStatus release() override
    {
        return {};
    }

    std::int64_t calls = 0;
};

ToolResult run(const TestRegistry &registry, std::string_view id, const Values &inputs)
{
    const auto *descriptor = [&]() -> const ToolFactoryDescriptor *
    {
        for (const auto &candidate : registry.descriptors)
            if (id == candidate.tool_id)
                return &candidate;
        return nullptr;
    }();
    assert(descriptor);
    ToolStatus   status;
    ToolServices services;
    auto        *instance = descriptor->create(&services, &status);
    assert(instance && status.code == 0);
    // The module's destroy entry is the only cleanup that always runs: releasing here as well keeps
    // the failure case from leaking the instance when an author exception skips the normal path.
    struct OwnedInstance
    {
        const ToolFactoryDescriptor *descriptor;
        IToolInstance               *instance;
        bool                         released = false;

        ~OwnedInstance()
        {
            if (!released)
            {
                try
                {
                    instance->release();
                }
                catch (...)
                {
                }
            }
            descriptor->destroy(instance);
        }
    } owned{descriptor, instance};
    const auto initialized = instance->initialize({});
    assert(initialized.code == 0);
    auto result = instance->execute(inputs);
    assert(instance->release().code == 0);
    owned.released = true;
    return result;
}
} // namespace

int main()
try
{
    TestRegistry registry;
    assert(registerFunction<&addOne>(registry, "test.function.add_one").code == 0);
    assert(registerFunction<&failLoudly>(registry, "test.function.fail").code == 0);
    assert(registerClass<Counter>(registry, "test.class.counter").code == 0);
    assert(registerFunction<&addOne>(registry, "test.function.add_one").code != 0);
    assert(registry.descriptors.size() == 3);

    // A function has no business state: two nodes interleaved see only their own inputs.
    const auto first  = run(registry, "test.function.add_one", {{1, std::int64_t{1}}});
    const auto second = run(registry, "test.function.add_one", {{1, std::int64_t{41}}});
    assert(std::get<std::int64_t>(first.outputs.at(2)) == 2);
    assert(std::get<std::int64_t>(first.outputs.at(kStatusCodePort)) == 0);
    assert(std::get<std::string>(first.outputs.at(kStatusMessagePort)) == "运行成功");
    assert(std::get<std::int64_t>(second.outputs.at(2)) == 42);
    assert(std::get<std::int64_t>(first.outputs.at(2)) == 2);
    assert(std::get<std::int64_t>(first.outputs.at(kStatusCodePort)) == 0);
    assert(std::get<std::string>(first.outputs.at(kStatusMessagePort)) == "运行成功");

    // A class keeps its instance counter, which is exactly why stateful tools stay classes: the
    // same instance advances twice and a second instance starts over.
    {
        const auto *counter = [&]() -> const ToolFactoryDescriptor *
        {
            for (const auto &candidate : registry.descriptors)
                if (std::string_view(candidate.tool_id) == "test.class.counter")
                    return &candidate;
            return nullptr;
        }();
        assert(counter);
        ToolStatus   status;
        ToolServices services;
        auto        *first = counter->create(&services, &status);
        assert(first && status.code == 0 && first->initialize({}).code == 0);
        assert(std::get<std::int64_t>(first->execute({}).outputs.at(2)) == 1);
        assert(std::get<std::int64_t>(first->execute({}).outputs.at(2)) == 2);
        auto *second = counter->create(&services, &status);
        assert(second && status.code == 0 && second->initialize({}).code == 0);
        assert(std::get<std::int64_t>(second->execute({}).outputs.at(2)) == 1);
        assert(first->release().code == 0 && second->release().code == 0);
        counter->destroy(first);
        counter->destroy(second);
    }

    // Author exceptions reach the host's existing diagnostic path unchanged.
    bool propagated = false;
    try
    {
        run(registry, "test.function.fail", {});
    }
    catch (const ContractError &error)
    {
        propagated = std::string(error.what()) == "函数内业务失败";
    }
    assert(propagated);

    // A function tool rejects init parameters instead of silently ignoring configuration.
    const auto &descriptor = registry.descriptors.front();
    ToolStatus  status;
    ToolServices services;
    auto        *instance = descriptor.create(&services, &status);
    assert(instance);
    assert(instance->initialize({{1, 2.0}}).code != 0);
    descriptor.destroy(instance);

    // A missing service block is reported, never dereferenced.
    status = {};
    assert(descriptor.create(nullptr, &status) == nullptr && status.code != 0);
    assert(descriptor.create(nullptr, nullptr) == nullptr);

    std::cout << "Function and class author entries share the module registration protocol\n";
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}

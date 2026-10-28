#pragma once

// Author helper for stateless tools. A function is adapted to the existing IToolInstance
// interface, so scheduling, cancellation, readback, teardown and ownership keep one code path.
// The public ABI (ToolFactoryDescriptor, IToolRegistry, ModuleApi) is unchanged.

#include "visionrt/contracts/Registry.h"

#include <exception>
#include <type_traits>
#include <utility>

namespace visionrt {
namespace detail {

template<class Factory>
IToolInstance *VT_CALL createWith(Factory factory, ToolStatus *status) noexcept
{
    try
    {
        if (status)
            *status = {};
        return factory();
    }
    catch (const std::exception &error)
    {
        if (status)
            *status = {kContractViolationStatus, "工具实例创建失败", error.what()};
    }
    catch (...)
    {
        if (status)
            *status = {kContractViolationStatus, "工具实例创建发生未知异常"};
    }
    return nullptr;
}

inline void VT_CALL destroyInstance(IToolInstance *instance) noexcept
{
    delete instance;
}

} // namespace detail

// A stateless function keeps no business state between calls: everything it needs arrives as this
// call's inputs. Services are copied once per instance; nothing is shared between nodes.
template<auto Function>
    requires std::is_same_v<decltype(Function), ToolResult (*)(const Values &, const ToolServices &)>
class FunctionToolInstance final : public IToolInstance
{
public:
    explicit FunctionToolInstance(ToolServices services)
        : services_(std::move(services))
    {
    }

    ToolStatus initialize(const Values &initParams) override
    {
        // Configuration belongs to the call's inputs. Accepting init parameters here would hide a
        // declaration/author mismatch instead of reporting it.
        return initParams.empty() ? ToolStatus{} : ToolStatus{kContractViolationStatus, "函数工具不接受初始化参数"};
    }

    ToolResult execute(const Values &inputs) override
    {
        return Function(inputs, services_);
    }

    ToolStatus release() override
    {
        return {};
    }

private:
    ToolServices services_;
};

namespace detail {

template<auto Function>
IToolInstance *VT_CALL createFunction(const ToolServices *services, ToolStatus *status) noexcept
{
    if (!services)
    {
        if (status)
            *status = {kContractViolationStatus, "工具服务不能为空"};
        return nullptr;
    }
    return createWith([&] { return new FunctionToolInstance<Function>(*services); }, status);
}

template<class Tool>
IToolInstance *VT_CALL createClass(const ToolServices *services, ToolStatus *status) noexcept
{
    if (!services)
    {
        if (status)
            *status = {kContractViolationStatus, "工具服务不能为空"};
        return nullptr;
    }
    return createWith([&] { return new Tool(*services); }, status);
}

} // namespace detail

// Only function pointers are accepted: a capturing lambda or std::function would smuggle
// per-registration state into a stateless contract.
template<auto Function, std::size_t Size>
    requires std::is_same_v<decltype(Function), ToolResult (*)(const Values &, const ToolServices &)>
ToolStatus registerFunction(IToolRegistry &registry, const char (&toolId)[Size]) noexcept
{
    static_assert(Size > 1, "工具 ID 不能为空");
    return registry.add(ToolFactoryDescriptor{toolId, &detail::createFunction<Function>,
                                              &detail::destroyInstance});
}

// Same creation/destruction wrappers for stateful classes: read-image cursors, model resources and
// counters keep their instance.
template<class Tool, std::size_t Size>
    requires std::is_base_of_v<IToolInstance, Tool> && std::is_constructible_v<Tool, ToolServices>
ToolStatus registerClass(IToolRegistry &registry, const char (&toolId)[Size]) noexcept
{
    static_assert(Size > 1, "工具 ID 不能为空");
    return registry.add(
        ToolFactoryDescriptor{toolId, &detail::createClass<Tool>, &detail::destroyInstance});
}

} // namespace visionrt

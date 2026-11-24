// Intentionally reusable: the caller supplies one definition file, header and catalog namespace.
namespace VRT_ENUM_CATALOG {
#define VRT_ENUM_BEGIN(name, underlying, ...) inline constexpr visionrt::codegen::EnumMember name##_members[]{
#define VRT_ENUM_VALUE(name, value, ...) {#name, value __VA_OPT__(,) __VA_ARGS__},
#define VRT_ENUM_END(name) };
#include VRT_ENUM_FILE
#undef VRT_ENUM_BEGIN
#undef VRT_ENUM_VALUE
#undef VRT_ENUM_END

inline constexpr visionrt::codegen::EnumDefinition enums[]{
#define VRT_ENUM_BEGIN(name, underlying, ...) {#name, VRT_ENUM_HEADER, name##_members __VA_OPT__(,) __VA_ARGS__},
#define VRT_ENUM_VALUE(name, value, ...)
#define VRT_ENUM_END(name)
#include VRT_ENUM_FILE
#undef VRT_ENUM_BEGIN
#undef VRT_ENUM_VALUE
#undef VRT_ENUM_END
};
}
#undef VRT_ENUM_FILE
#undef VRT_ENUM_HEADER
#undef VRT_ENUM_CATALOG

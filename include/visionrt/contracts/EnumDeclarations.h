// Intentionally reusable: expand the caller's definition file in its owning namespace.
#define VRT_ENUM_BEGIN(name, underlying, ...) enum class name : underlying {
#define VRT_ENUM_VALUE(name, value, ...) name = value,
#define VRT_ENUM_END(name) };
#include VRT_ENUM_FILE
#undef VRT_ENUM_BEGIN
#undef VRT_ENUM_VALUE
#undef VRT_ENUM_END
#undef VRT_ENUM_FILE

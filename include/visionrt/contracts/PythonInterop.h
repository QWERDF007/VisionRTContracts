#pragma once

#include "visionrt/contracts/Value.h"

#include <pybind11/pybind11.h>

namespace visionrt {

// Converts a PortValue to a read-only Python object (NumPy array, list of arrays, or scalar).
// The caller holds the GIL for the call and for every returned Python reference.
// Array inputs retain only C++ owners; outputs never retain Python references.
VT_EXPORT pybind11::object toPythonReadonly(const PortValue& value);

// Converts a Python object back to PortValue matching the expected TypeSpec.
// C++ deep-copies or takes ownership into pure C++ storage; no Python reference is retained.
VT_EXPORT PortValue fromPython(const TypeSpec& expected, pybind11::handle value);

#if defined(VISIONRT_TEST_PROBES)
VT_EXPORT std::size_t pythonLeafImageCopyCount() noexcept;
VT_EXPORT void resetPythonLeafImageCopyCount() noexcept;
VT_EXPORT std::size_t pythonLeafNumericCopyCount() noexcept;
VT_EXPORT void resetPythonLeafNumericCopyCount() noexcept;
#endif

} // namespace visionrt

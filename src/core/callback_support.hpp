#pragma once

#include "dof_extension/extension_api.hpp"

#include <cstdint>

namespace dofext {

// Minimal VM bridge shared by the three callback groups.
ExtensionContext* Context();

int Error(VmHandle vm, const wchar_t* message);
bool GetInt(VmHandle vm, int index, std::int32_t& value);
bool GetFloat(VmHandle vm, int index, float& value);
bool GetBool(VmHandle vm, int index, bool& value);
bool GetString(VmHandle vm, int index, const wchar_t*& value);
bool GetObjectArgument(VmHandle vm, int index, std::uintptr_t& value);
int PushInt(VmHandle vm, std::int32_t value);
int PushFloat(VmHandle vm, float value);
int PushString(VmHandle vm, const wchar_t* value);
int PushObject(VmHandle vm, std::uintptr_t value);

} // namespace dofext

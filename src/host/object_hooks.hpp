#pragma once

#include <cstdint>

namespace dofext {

bool InstallObjectHooks();
void RemoveObjectHooks();
bool ObjectHooksInstalled();
void ClearObjectPaths();

bool RegisterObjectPath(const wchar_t* object_name,
                        std::int32_t object_index,
                        const wchar_t* member_path);

std::int32_t SkillSlotOffsetX();
std::int32_t SkillSlotOffsetY();

} // namespace dofext

#pragma once

#include "dof_extension/extension_api.hpp"

#include <cstdint>

namespace dofext::host_detail {

template <typename T>
T ReadAbs(std::uintptr_t address) {
    return address ? *reinterpret_cast<const T*>(address) : T{};
}

std::uintptr_t AbsoluteResolvePath(std::uintptr_t base, const wchar_t* path);
std::int32_t AbsoluteDecode(std::uintptr_t field);
void AbsoluteEncode(std::uintptr_t field, std::int32_t signed_value);
std::int32_t AbsoluteScreenX();
std::int32_t AbsoluteSkillCooldown(std::uintptr_t skill);

void AbsoluteDrawSkillIcon(const DrawSkillArgs& args);
void AbsoluteDrawItemSlot(std::int32_t slot_id, std::int32_t x,
                          std::int32_t y, std::int32_t flags);
void AbsoluteDrawText(std::int32_t x, std::int32_t y,
                      std::int32_t color, const wchar_t* text);
void AbsoluteDrawTips(std::int32_t x, std::int32_t y,
                      std::int32_t width, std::int32_t style,
                      const wchar_t* text, std::int32_t arg6,
                      std::int32_t arg7);
bool AbsoluteDrawImage(const DrawImageArgs& args);
void AbsoluteNotice(const wchar_t* text, std::int32_t type, std::int32_t channel);
void AbsolutePacketBegin(std::int32_t message_id);
void AbsolutePacketWrite(std::int32_t value, std::int32_t width_type);
void AbsolutePacketSend();

std::int32_t AbsoluteLookupInfo(const wchar_t* key);
std::int32_t AbsoluteGetInfoFromData(std::int32_t index);
bool AbsoluteSaveSkillData(std::int32_t a, std::int32_t b,
                           std::int32_t c, std::int32_t d);

const wchar_t* AbsoluteCharacterName();
std::int32_t AbsoluteGameState();
std::int32_t AbsoluteTownIndex();
std::int32_t AbsoluteArea();
std::int32_t AbsoluteItemSlotIndex(std::uintptr_t object, std::int32_t slot_type);
bool AbsolutePushObjectByName(const wchar_t* object_name,
                              std::int32_t object_index,
                              const wchar_t* member_path);

} // namespace dofext::host_detail

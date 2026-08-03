#include "core/callback_support.hpp"
#include "common/offset_path.hpp"
#include "core/game_data_layout.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cwchar>
#include <limits>
#include <string>
#include <vector>

namespace dofext {
namespace {

template <typename T>
T Read(std::uintptr_t address) {
    return address ? *reinterpret_cast<const T*>(address) : T{};
}

template <typename T>
void Write(std::uintptr_t address, T value) {
    if (address) *reinterpret_cast<T*>(address) = value;
}

std::uintptr_t AddOffset(std::uintptr_t base, std::int64_t offset) {
    if (!base) return 0;
    const auto result = static_cast<std::int64_t>(base) + offset;
    if (result <= 0 ||
        static_cast<std::uint64_t>(result) > std::numeric_limits<std::uintptr_t>::max()) {
        return 0;
    }
    return static_cast<std::uintptr_t>(result);
}

std::uintptr_t ResolvePath(std::uintptr_t object, const wchar_t* path) {
    if (!Context() || !Context()->host.resolve_path) return 0;
    return Context()->host.resolve_path(object, path ? path : L"");
}

std::wstring MakeDecimalNibbleEscapedWide(const wchar_t* value) {
    if (!value) return {};
    const int wide_length = static_cast<int>(std::wcslen(value));
    const int byte_length = WideCharToMultiByte(
        CP_UTF8, 0, value, wide_length, nullptr, 0, nullptr, nullptr);
    if (byte_length <= 0) return {};
    std::string utf8(static_cast<std::size_t>(byte_length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, wide_length,
                        &utf8[0], byte_length, nullptr, nullptr);
    std::wstring result;
    result.reserve(utf8.size() * 6);
    for (const unsigned char byte : utf8) {
        result += L"\\x";
        result += std::to_wstring(byte >> 4);
        result += std::to_wstring(byte & 0x0F);
    }
    return result;
}

std::uintptr_t ResolveEncodedPath(std::uintptr_t object, const wchar_t* path) {
    const auto offsets = ParseOffsetPath(path);
    if (offsets.empty()) return 0;
    std::uintptr_t current = object;
    for (std::size_t i = 0; i + 1 < offsets.size(); ++i) {
        current = Read<std::uintptr_t>(AddOffset(current, offsets[i]));
        if (!current) return 0;
    }
    return AddOffset(current, offsets.back());
}

std::uintptr_t ResolvePointerPath(std::uintptr_t object, const wchar_t* path) {
    const auto offsets = ParseOffsetPath(path);
    if (offsets.empty()) return 0;
    std::uintptr_t current = object;
    for (const auto offset : offsets) {
        current = Read<std::uintptr_t>(AddOffset(current, offset));
        if (!current) return 0;
    }
    return current;
}

std::int32_t Decode(std::uintptr_t address) {
    if (!address) return 0;
    if (Context() && Context()->host.decode_i32) {
        return Context()->host.decode_i32(address);
    }
    return Read<std::int32_t>(address);
}

void Encode(std::uintptr_t address, std::int32_t value) {
    if (!address) return;
    if (Context() && Context()->host.encode_i32) {
        Context()->host.encode_i32(address, value);
    } else {
        Write<std::int32_t>(address, value);
    }
}

std::uintptr_t TableEntry(std::uintptr_t object, std::int32_t table) {
    if (!object || table < -static_cast<std::int32_t>(layout::kStaticTableBias)) return 0;
    const auto slot_index = static_cast<std::int64_t>(table) + layout::kStaticTableBias;
    return Read<std::uintptr_t>(AddOffset(object, 4 * slot_index));
}

std::uintptr_t StaticDataFieldFromEntry(std::uintptr_t entry, std::int32_t field) {
    if (!entry || field < 0) return 0;
    const auto data = Read<std::uintptr_t>(AddOffset(entry, layout::kStaticDataOffset));
    return AddOffset(data, 12LL * field);
}

std::uintptr_t StaticDataField(std::uintptr_t object, std::int32_t table,
                               std::int32_t field) {
    return StaticDataFieldFromEntry(TableEntry(object, table), field);
}

std::uintptr_t MirrorEntry(std::uintptr_t object, std::uintptr_t primary_entry) {
    if (!primary_entry) return 0;
    const auto mirror_index = Decode(AddOffset(primary_entry,
        layout::kMirrorIndexFieldOffset));
    return mirror_index == -1 ? 0 : TableEntry(object, mirror_index);
}

std::uintptr_t LevelDataFieldFromEntry(std::uintptr_t entry,
                                       std::int32_t level,
                                       std::int32_t field) {
    if (!entry || level < 0 || field <= 0) return 0;
    const auto levels = Read<std::uintptr_t>(AddOffset(entry, layout::kLevelDataOffset));
    const auto level_slot = AddOffset(levels, 20LL * level + 4);
    const auto level_data = Read<std::uintptr_t>(level_slot);
    return AddOffset(level_data, 12LL * (field - 1));
}

std::uintptr_t LevelDataField(std::uintptr_t object, std::int32_t table,
                              std::int32_t level, std::int32_t field) {
    return LevelDataFieldFromEntry(TableEntry(object, table), level, field);
}

} // namespace


int DOFEXT_CDECL cqx_readValue(VmHandle vm) {
    std::int32_t object_value = 0;
    const wchar_t* path = nullptr;
    bool encoded = false;
    std::int32_t kind = 0;
    if (!GetInt(vm, 2, object_value) || !GetString(vm, 3, path) ||
        !GetBool(vm, 4, encoded) || !GetInt(vm, 5, kind)) {
        return Error(vm, L"cqx_readValue: pointer, path, encoded and kind expected");
    }
    const auto object = static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(object_value));
    const bool empty_path = path && !*path;
    const auto field = empty_path ? object : ResolvePath(object, path);
    if (kind == 1) {
        const wchar_t* value = empty_path
            ? reinterpret_cast<const wchar_t*>(object)
            : Read<const wchar_t*>(field);
        return PushString(vm, value);
    }
    if (!field) return kind == 2 ? PushFloat(vm, 0.0f) : PushInt(vm, 0);
    if (kind == 2) {
        const auto raw = encoded ? Decode(field) : Read<std::int32_t>(field);
        float value = 0.0f;
        static_assert(sizeof(value) == sizeof(raw));
        std::memcpy(&value, &raw, sizeof(value));
        return PushFloat(vm, value);
    }
    return PushInt(vm, encoded ? Decode(field) : Read<std::int32_t>(field));
}


int DOFEXT_CDECL cqx_getStaticData(VmHandle vm) {
    std::uintptr_t object = 0;
    std::int32_t table = 0, field = 0;
    if (!GetObjectArgument(vm, 2, object) || !GetInt(vm, 3, table) || !GetInt(vm, 4, field)) {
        return Error(vm, L"failed to get DataS information");
    }
    return PushInt(vm, Decode(StaticDataField(object, table, field)));
}

int DOFEXT_CDECL cqx_getLeveData(VmHandle vm) {
    std::uintptr_t object = 0;
    std::int32_t table = 0, level = 0, field = 0;
    if (!GetObjectArgument(vm, 2, object) || !GetInt(vm, 3, table) ||
        !GetInt(vm, 4, level) || !GetInt(vm, 5, field)) {
        return Error(vm, L"failed to get DataL information");
    }
    const auto raw = static_cast<std::uint32_t>(Decode(LevelDataField(object, table, level, field)));
    float value = 0.0f;
    std::memcpy(&value, &raw, sizeof(value));
    return PushFloat(vm, value);
}

int DOFEXT_CDECL cqx_setStaticData(VmHandle vm) {
    std::uintptr_t object = 0;
    std::int32_t table = 0, field = 0, value = 0;
    if (!GetObjectArgument(vm, 2, object) || !GetInt(vm, 3, table) ||
        !GetInt(vm, 4, field) || !GetInt(vm, 5, value)) {
        return Error(vm, L"failed to set DataS information");
    }
    const auto entry = TableEntry(object, table);
    Encode(StaticDataFieldFromEntry(entry, field), value);
    const auto mirror = MirrorEntry(object, entry);
    if (mirror) Encode(StaticDataFieldFromEntry(mirror, field), value);
    return 0;
}

int DOFEXT_CDECL cqx_setLevelData(VmHandle vm) {
    std::uintptr_t object = 0;
    std::int32_t table = 0, level = 0, field = 0, raw_value = 0;
    if (!GetObjectArgument(vm, 2, object) || !GetInt(vm, 3, table) ||
        !GetInt(vm, 4, level) || !GetInt(vm, 5, field) ||
        !GetInt(vm, 6, raw_value)) {
        return Error(vm, L"failed to set DataL information");
    }
    const float float_value = static_cast<float>(raw_value);
    std::int32_t encoded_bits = 0;
    std::memcpy(&encoded_bits, &float_value, sizeof(encoded_bits));
    const auto entry = TableEntry(object, table);
    Encode(LevelDataFieldFromEntry(entry, level, field), encoded_bits);
    const auto mirror = MirrorEntry(object, entry);
    if (mirror) Encode(LevelDataFieldFromEntry(mirror, level, field), encoded_bits);
    return 0;
}

int DOFEXT_CDECL cqx_saveDataSkillFile(VmHandle vm) {
    std::int32_t a = 0, b = 0, c = 0, d = 0;
    if (!GetInt(vm, 2, a) || !GetInt(vm, 3, b) ||
        !GetInt(vm, 4, c) || !GetInt(vm, 5, d)) {
        return Error(vm, L"cqx_saveDataSkillFile: invalid arguments");
    }
    if (Context()->host.save_skill_data) Context()->host.save_skill_data(a, b, c, d);
    return 0;
}

int DOFEXT_CDECL cqx_getinfoFromData(VmHandle vm) {
    std::int32_t index = 0;
    if (!GetInt(vm, 2, index)) return Error(vm, L"cqx_getinfoFromData: index expected");
    if (!Context()->host.get_info_from_data) return Error(vm, L"get_info_from_data is not bound");
    return PushInt(vm, Context()->host.get_info_from_data(index));
}

int DOFEXT_CDECL cqx_GetChrName(VmHandle vm) {
    if (!Context() || !Context()->host.get_character_name) return Error(vm, L"get_character_name is not bound");
    return PushString(vm, Context()->host.get_character_name());
}

int DOFEXT_CDECL cqx_getObjectAnyInfo(VmHandle vm) {
    std::uintptr_t object = 0;
    const wchar_t* path = nullptr;
    std::int32_t kind = 0;
    if (!GetObjectArgument(vm, 2, object) || !GetString(vm, 3, path) || !GetInt(vm, 4, kind)) {
        return Error(vm, L"failed to get object any information");
    }
    if (path && !*path) return PushInt(vm, -1);
    switch (kind) {
    case 0:
        return PushInt(vm, static_cast<std::int32_t>(ResolvePointerPath(object, path)));
    case 1:
        return PushInt(vm, Decode(ResolveEncodedPath(object, path)));
    case 2: {
        const auto raw = static_cast<std::uint32_t>(Decode(ResolveEncodedPath(object, path)));
        float value = 0.0f;
        std::memcpy(&value, &raw, sizeof(value));
        return PushFloat(vm, value);
    }
    case 3:
        return PushString(vm, reinterpret_cast<const wchar_t*>(
            ResolvePointerPath(object, path)));
    default:
        return PushInt(vm, -1);
    }
}

int DOFEXT_CDECL cqx_getObjectInfo(VmHandle vm) {
    std::uintptr_t object = 0;
    std::int32_t offset = 0, kind = 0;
    if (!GetObjectArgument(vm, 2, object) || !GetInt(vm, 3, offset) || !GetInt(vm, 4, kind)) {
        return Error(vm, L"failed to get object information");
    }
    const auto field = AddOffset(object, offset);
    if (!field) return Error(vm, L"cqx_getObjectInfo: null object or invalid offset");
    switch (kind) {
    case 1: return PushInt(vm, Decode(field));
    case 2: {
        const auto raw = static_cast<std::uint32_t>(Decode(field));
        float value = 0.0f;
        std::memcpy(&value, &raw, sizeof(value));
        return PushFloat(vm, value);
    }
    case 3: return PushString(vm, Read<const wchar_t*>(field));
    case 4: {
        const auto escaped = MakeDecimalNibbleEscapedWide(
            Read<const wchar_t*>(field));
        return PushString(vm, escaped.c_str());
    }
    default: return PushInt(vm, Read<std::int32_t>(field));
    }
}

int DOFEXT_CDECL cqx_setObjectData(VmHandle vm) {
    std::uintptr_t object = 0;
    std::int32_t offset = 0, value = 0;
    bool raw_write = false;
    if (!GetObjectArgument(vm, 2, object) || !GetInt(vm, 3, offset) ||
        !GetBool(vm, 4, raw_write) || !GetInt(vm, 5, value)) {
        return Error(vm, L"failed to get object");
    }
    const auto field = AddOffset(object, offset);
    if (!field) return 0;
    if (raw_write) Write<std::int32_t>(field, value); else Encode(field, value);
    return 0;
}

int DOFEXT_CDECL cqx_getObjectInfo2nd(VmHandle vm) {
    std::uintptr_t object = 0;
    std::int32_t first = 0, second = 0, kind = 0;
    if (!GetObjectArgument(vm, 2, object) || !GetInt(vm, 3, first) ||
        !GetInt(vm, 4, second) || !GetInt(vm, 5, kind)) {
        return Error(vm, L"failed to get object information2");
    }
    if (kind == 0) {
        const std::int32_t offsets[]{first, second};
        std::uintptr_t value = object;
        for (const auto current : offsets) {
            value = AddOffset(Read<std::uintptr_t>(value), current);
        }
        return PushString(vm, reinterpret_cast<const wchar_t*>(value));
    }
    const auto child = Read<std::uintptr_t>(AddOffset(object, first));
    const auto field = AddOffset(child, second);
    if (!field) return Error(vm, L"cqx_getObjectInfo2nd: null object chain or invalid offset");
    if (kind == 1) return PushInt(vm, Decode(field));
    if (kind == 2) {
        const auto raw = static_cast<std::uint32_t>(Decode(field));
        float value = 0.0f;
        std::memcpy(&value, &raw, sizeof(value));
        return PushFloat(vm, value);
    }
    return PushInt(vm, Read<std::int32_t>(field));
}

int DOFEXT_CDECL cqx_setObjectInfo2nd(VmHandle vm) {
    std::uintptr_t object = 0;
    std::int32_t first = 0, second = 0, value = 0;
    bool encoded = false;
    if (!GetObjectArgument(vm, 2, object) || !GetInt(vm, 3, first) ||
        !GetInt(vm, 4, second) || !GetBool(vm, 5, encoded) ||
        !GetInt(vm, 6, value)) {
        return Error(vm, L"failed to set object information2");
    }
    const auto child = Read<std::uintptr_t>(AddOffset(object, first));
    const auto field = AddOffset(child, second);
    if (!field) return 0;
    if (encoded) Encode(field, value); else Write<std::int32_t>(field, value);
    return 0;
}

int DOFEXT_CDECL cqx_GetGameState(VmHandle vm) {
    if (!Context() || !Context()->host.get_game_state) return Error(vm, L"get_game_state is not bound");
    return PushInt(vm, Context()->host.get_game_state());
}

int DOFEXT_CDECL cqx_GetTownIndex(VmHandle vm) {
    if (!Context() || !Context()->host.get_town_index) return Error(vm, L"get_town_index is not bound");
    return PushInt(vm, Context()->host.get_town_index());
}

int DOFEXT_CDECL cqx_GetAreaIndex(VmHandle vm) {
    if (!Context() || !Context()->host.get_area_index) return Error(vm, L"get_area_index is not bound");
    return PushInt(vm, Context()->host.get_area_index());
}

int DOFEXT_CDECL cqx_getItemSlotIndex(VmHandle vm) {
    std::uintptr_t object = 0;
    std::int32_t slot_type = 0;
    if (!GetObjectArgument(vm, 2, object) || !GetInt(vm, 3, slot_type)) {
        return Error(vm, L"failed to get object ItemSlotIndex");
    }
    if (!Context()->host.get_item_slot_index) return Error(vm, L"get_item_slot_index is not bound");
    return PushInt(vm, Context()->host.get_item_slot_index(object, slot_type));
}

} // namespace dofext

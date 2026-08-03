#include "host/absolute_host_functions.hpp"
#include "common/offset_path.hpp"
#include "host/host_addresses.hpp"
#include "host/object_hooks.hpp"

#include <initializer_list>

namespace dofext::host_detail {

template <typename T>
void WriteAbs(std::uintptr_t address, T value) {
    if (address) *reinterpret_cast<T*>(address) = value;
}

std::uintptr_t Add(std::uintptr_t base, std::int32_t offset) {
    return base ? base + static_cast<std::uintptr_t>(offset) : 0;
}

std::uintptr_t Walk(std::uintptr_t start_address,
                    std::initializer_list<std::int32_t> offsets) {
    std::uintptr_t address = start_address;
    for (const auto offset : offsets) {
        address = Add(ReadAbs<std::uintptr_t>(address), offset);
        if (!address) return 0;
    }
    return ReadAbs<std::uintptr_t>(address);
}

std::uintptr_t AbsoluteResolvePath(std::uintptr_t base, const wchar_t* path) {
    if (!base || !path || !*path) return 0;
    const auto offsets = ParseOffsetPath(path);
    std::uintptr_t current = base;
    for (const auto offset : offsets) {
        current = Add(ReadAbs<std::uintptr_t>(current), offset);
        if (!current) return 0;
    }
    return current;
}

std::int32_t AbsoluteDecode(std::uintptr_t field) {
    // Exact arithmetic recovered from cqx_readValue/sub_664F3CE0.
    if (!field) return 0;
    const auto key = ReadAbs<std::uint32_t>(field);
    const auto root = ReadAbs<std::uintptr_t>(address::kEncodedFieldTables);
    const auto bucket = ReadAbs<std::uintptr_t>(root + 36u +
                                               4u * static_cast<std::uint16_t>(key >> 16));
    const auto mask = ReadAbs<std::uint16_t>(bucket +
                                             4u * static_cast<std::uint16_t>(key) +
                                             address::kEncodedLookupBias);
    const auto payload = ReadAbs<std::uint32_t>(field + address::kEncodedPayloadOffset);
    return static_cast<std::int32_t>(payload ^ (0x10001u * mask));
}

void AbsoluteEncode(std::uintptr_t field, std::int32_t signed_value) {
    // Near line-for-line recovery of sub_664F3C30. Valid game pointers assumed.
    if (!field) return;
    auto counter = ReadAbs<std::uint32_t>(address::kEncodeCounter) + 1u;
    WriteAbs<std::uint32_t>(address::kEncodeCounter, counter);

    const auto word_a = ReadAbs<std::uint16_t>(address::kEncryptWordTableA +
                                               2u * (counter >> 8));
    const auto word_b = ReadAbs<std::uint16_t>(address::kEncryptWordTableB +
                                               2u * counter);
    const auto salt = static_cast<std::uint16_t>(word_a ^ word_b);
    const auto value = static_cast<std::uint32_t>(signed_value);
    WriteAbs<std::uint32_t>(field + address::kEncodedPayloadOffset,
                            value ^ (0x10001u * salt));

    const auto key = ReadAbs<std::uint32_t>(field);
    const auto mixed_low = static_cast<std::uint16_t>(value) +
                           static_cast<std::uint16_t>(signed_value >> 16);
    const auto encoded_key = static_cast<std::uint32_t>(salt) +
        (static_cast<std::uint32_t>(salt ^ mixed_low) << 16);
    const auto root = ReadAbs<std::uintptr_t>(address::kEncodedFieldTables);
    const auto bucket = ReadAbs<std::uintptr_t>(root + 36u +
                                               4u * static_cast<std::uint16_t>(key >> 16));
    WriteAbs<std::uint32_t>(bucket + 4u *
        (static_cast<std::uint16_t>(key) + 2117u), encoded_key);
}

std::int32_t AbsoluteScreenX() {
    const auto value = ReadAbs<std::int32_t>(address::kScreenXGlobal);
    // Current callback 0x6511A2A0 falls back to 800 if both its guarded read
    // and protected fallback helper return zero.
    return value ? value : 800;
}

std::int32_t AbsoluteSkillCooldown(std::uintptr_t skill) {
    using Fn = std::int32_t(__thiscall*)(void*);
    return reinterpret_cast<Fn>(address::kSkillCooldown)(
        reinterpret_cast<void*>(skill));
}

const wchar_t* AbsoluteCharacterName() {
    return reinterpret_cast<const wchar_t*>(Walk(address::kCharacterGlobal, {600}));
}

std::int32_t AbsoluteGameState() {
    const auto root = ReadAbs<std::int32_t>(address::kGameStateGlobal);
    std::uintptr_t value_address = 40u;
    if (root != -20) {
        const auto object = ReadAbs<std::int32_t>(
            static_cast<std::uintptr_t>(root + 20));
        if (object == -40) return 0;
        value_address = static_cast<std::uintptr_t>(object + 40);
    }
    return ReadAbs<std::int32_t>(value_address);
}

std::int32_t AbsoluteTownIndex() {
    return static_cast<std::int32_t>(Walk(address::kTownAreaRootGlobal, {172, 212}));
}

std::int32_t AbsoluteArea() {
    const auto area_object = Walk(address::kTownAreaRootGlobal, {172, 216});
    return area_object ? ReadAbs<std::int32_t>(area_object) : 0;
}

std::int32_t AbsoluteItemSlotIndex(std::uintptr_t object, std::int32_t slot_type) {
    if (static_cast<std::int32_t>(object) == -21236) return 0;
    const auto first = ReadAbs<std::uintptr_t>(object + address::kItemSlotRootOffset);
    if (!first) return 0;
    const auto table = ReadAbs<std::uintptr_t>(first + address::kItemSlotTableOffset);
    if (!table) return 0;
    std::uint32_t slot_offset = 12;
    switch (slot_type) {
    case 1: slot_offset = 16; break;
    case 2: slot_offset = 20; break;
    case 3: slot_offset = 24; break;
    case 4: slot_offset = 28; break;
    case 5: slot_offset = 32; break;
    default: break;
    }
    const auto slot = ReadAbs<std::int32_t>(table + slot_offset);
    if (!slot || slot == -28) return 0;
    return ReadAbs<std::int32_t>(static_cast<std::uintptr_t>(
        slot + static_cast<std::int32_t>(address::kItemSlotValueOffset)));
}

bool AbsolutePushObjectByName(const wchar_t* object_name, std::int32_t object_index,
                              const wchar_t* member_path) {
    return RegisterObjectPath(object_name, object_index, member_path);
}

} // namespace dofext::host_detail

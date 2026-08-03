#include "runtime/closure_installer.hpp"
#include "host/game_squirrel_vm_adapter.hpp"
#include "resource.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace dofext {
namespace {

constexpr DWORD kMaximumPayloadBytes = 8u * 1024u * 1024u;
constexpr std::uint32_t kExpectedVersion = 1u;
constexpr std::uint32_t kExpectedClosureCount = 1803u;
constexpr std::uint32_t kMaximumNameBytes = 63u;
constexpr std::uint32_t kMaximumStreamBytes = 1024u * 1024u;

bool ReadU32(const std::uint8_t* data, std::size_t size,
             std::size_t& offset, std::uint32_t& value) {
    if (!data || offset > size || size - offset < sizeof(value)) return false;
    std::memcpy(&value, data + offset, sizeof(value));
    offset += sizeof(value);
    return true;
}

bool ReadRecordHeader(const std::uint8_t* data, std::size_t size,
                      std::size_t& offset, std::uint32_t& closure_id,
                      std::uint32_t& name_size,
                      std::uint32_t& stream_size) {
    return ReadU32(data, size, offset, closure_id) &&
           ReadU32(data, size, offset, name_size) &&
           ReadU32(data, size, offset, stream_size) &&
           name_size > 0 && name_size <= kMaximumNameBytes &&
           stream_size >= 32 && stream_size <= kMaximumStreamBytes &&
           offset <= size && name_size <= size - offset &&
           stream_size <= size - offset - name_size;
}

struct ClosureRecord {
    std::array<wchar_t, 64> name{};
    const void* stream = nullptr;
    int stream_size = 0;
};

bool ParseClosureRecords(const std::uint8_t* data, std::size_t size,
                         std::size_t offset, std::uint32_t count,
                         std::vector<ClosureRecord>& records) {
    records.clear();
    records.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t closure_id = 0;
        std::uint32_t name_size = 0;
        std::uint32_t stream_size = 0;
        if (!ReadRecordHeader(data, size, offset, closure_id,
                              name_size, stream_size)) {
            return false;
        }

        char expected_name[64]{};
        const int expected_length = _snprintf_s(
            expected_name, sizeof(expected_name), _TRUNCATE,
            "ChangQingFunctionChar%u", closure_id);
        if (expected_length <= 0 ||
            static_cast<std::uint32_t>(expected_length) != name_size ||
            std::memcmp(data + offset, expected_name, name_size) != 0) {
            return false;
        }

        ClosureRecord record{};
        for (std::uint32_t i = 0; i < name_size; ++i) {
            const unsigned char ch = data[offset + i];
            if (ch < 0x20u || ch > 0x7Eu) return false;
            record.name[i] = static_cast<wchar_t>(ch);
        }
        offset += name_size;
        record.stream = data + offset;
        record.stream_size = static_cast<int>(stream_size);
        offset += stream_size;
        records.push_back(record);
    }
    return offset == size;
}

} // namespace

bool InstallGeneratedClosures(VmHandle vm, void* raw_module) {
    if (!vm || !raw_module || !IsCurrentGameVm(vm)) return false;
    const auto module = static_cast<HMODULE>(raw_module);
    HRSRC resource = FindResourceW(
        module, MAKEINTRESOURCEW(IDR_GENERATED_CLOSURES),
        MAKEINTRESOURCEW(10));
    if (!resource) return false;
    const DWORD byte_count = SizeofResource(module, resource);
    if (byte_count < 12u || byte_count > kMaximumPayloadBytes ||
        byte_count > static_cast<DWORD>(std::numeric_limits<int>::max())) {
        return false;
    }
    HGLOBAL loaded = LoadResource(module, resource);
    if (!loaded) return false;
    const auto* data = static_cast<const std::uint8_t*>(LockResource(loaded));
    if (!data || std::memcmp(data, "CQBC", 4) != 0) return false;

    std::size_t offset = 4u;
    std::uint32_t version = 0;
    std::uint32_t count = 0;
    if (!ReadU32(data, byte_count, offset, version) ||
        !ReadU32(data, byte_count, offset, count) ||
        version != kExpectedVersion || count != kExpectedClosureCount) {
        return false;
    }

    std::vector<ClosureRecord> records;
    if (!ParseClosureRecords(data, byte_count, offset, count, records)) {
        return false;
    }

    for (const auto& record : records) {
        if (!IsCurrentGameVm(vm)) return false;
        if (!CurrentGameRootSlotExists(vm, record.name.data()) &&
            !InstallCurrentGameSerializedClosure(
                vm, record.name.data(), record.stream, record.stream_size)) {
            return false;
        }
    }
    return true;
}

} // namespace dofext

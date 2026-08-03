#include "host/object_hooks.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace dofext {
namespace {

constexpr std::uintptr_t kCaptureObjectPatch = 0x01219E9Eu;
constexpr std::uintptr_t kObjectAllowedPatch = 0x00A2903Au;
constexpr std::uintptr_t kObjectPathPatch = 0x01219EC5u;
constexpr std::uintptr_t kObjectBranchPatch = 0x00A29032u;
constexpr std::uintptr_t kCaptureObjectOriginal = 0x0110F7C0u;
constexpr std::uintptr_t kPushObject = 0x006847B0u;
constexpr std::uintptr_t kExpectedObjectManager = 0x01D8D218u;
constexpr std::uintptr_t kConditionalRangePatch = 0x00A29026u;
constexpr std::uintptr_t kConditionalRangeAllowed = 0x00A29043u;
constexpr std::uintptr_t kConditionalRangeRejected = 0x00A2902Cu;
constexpr std::uintptr_t kAdditionalObjectPatch = 0x006BBEF3u;
constexpr std::uintptr_t kAdditionalObjectContinue = 0x006BBEF9u;
constexpr unsigned char kExpectedCaptureBytes[5]{0xE8, 0x1D, 0x59, 0xEF, 0xFF};
constexpr unsigned char kExpectedAllowedBytes[5]{0xE8, 0x41, 0x0E, 0x7F, 0x00};
constexpr unsigned char kExpectedPathBytes[5]{0x33, 0xC0, 0x8B, 0xE5, 0x5D};
constexpr unsigned char kExpectedBranchBytes[2]{0x77, 0x47};
constexpr unsigned char kExpectedConditionalRangeBytes[6]{0x8D, 0x96, 0x77, 0xA1, 0xFF, 0xFF};
constexpr unsigned char kExpectedAdditionalBytes[6]{0x8B, 0x88, 0xEC, 0x00, 0x00, 0x00};

std::unordered_map<std::int32_t, std::wstring> g_object_paths;
thread_local std::wstring g_object_path_result;
SRWLOCK g_object_paths_lock = SRWLOCK_INIT;
std::vector<std::pair<std::int32_t, std::int32_t>> g_configured_ranges;
std::uintptr_t g_current_object_manager = 0;
std::int32_t g_skill_slot_offset_x = 0;
std::int32_t g_skill_slot_offset_y = 0;
std::wstring g_skill_ini_path;
DWORD g_skill_offset_refresh_tick = 0;
bool g_hooks_installed = false;
bool g_config_loaded = false;
bool g_expand_enabled = false;
std::uintptr_t g_range_allowed_continuation = kConditionalRangeAllowed;
std::uintptr_t g_range_rejected_continuation = kConditionalRangeRejected;
std::uintptr_t g_additional_continuation = kAdditionalObjectContinue;

class SharedObjectPathsLock {
public:
    SharedObjectPathsLock() { AcquireSRWLockShared(&g_object_paths_lock); }
    ~SharedObjectPathsLock() { ReleaseSRWLockShared(&g_object_paths_lock); }
};

class ExclusiveObjectPathsLock {
public:
    ExclusiveObjectPathsLock() { AcquireSRWLockExclusive(&g_object_paths_lock); }
    ~ExclusiveObjectPathsLock() { ReleaseSRWLockExclusive(&g_object_paths_lock); }
};

struct PatchRecord {
    std::uintptr_t address{};
    std::array<unsigned char, 6> original{};
    std::size_t size{};
    bool saved{};
};

PatchRecord g_capture_patch{kCaptureObjectPatch};
PatchRecord g_allowed_patch{kObjectAllowedPatch};
PatchRecord g_path_patch{kObjectPathPatch};
PatchRecord g_branch_patch{kObjectBranchPatch};
PatchRecord g_range_patch{kConditionalRangePatch};
PatchRecord g_additional_patch{kAdditionalObjectPatch};

bool IsCommitted(std::uintptr_t address, std::size_t size) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto end = address + size;
    const auto region_end = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return end >= address && end <= region_end;
}

bool MatchesBytes(std::uintptr_t address, const unsigned char* expected,
                  std::size_t size) {
    return expected && IsCommitted(address, size) &&
           std::memcmp(reinterpret_cast<const void*>(address), expected, size) == 0;
}

bool WritePatch(PatchRecord& record, const void* bytes, std::size_t size) {
    if (!bytes || !size || size > record.original.size() ||
        !IsCommitted(record.address, size)) return false;
    if (!record.saved) {
        std::memcpy(record.original.data(), reinterpret_cast<const void*>(record.address), size);
        record.size = size;
        record.saved = true;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(record.address), size,
                        PAGE_EXECUTE_READWRITE, &old_protect)) return false;
    std::memcpy(reinterpret_cast<void*>(record.address), bytes, size);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(record.address), size);
    DWORD ignored = 0;
    VirtualProtect(reinterpret_cast<void*>(record.address), size, old_protect, &ignored);
    return true;
}

void RestorePatch(PatchRecord& record) {
    if (!record.saved || !record.size || !IsCommitted(record.address, record.size)) return;
    DWORD old_protect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(record.address), record.size,
                        PAGE_EXECUTE_READWRITE, &old_protect)) return;
    std::memcpy(reinterpret_cast<void*>(record.address), record.original.data(), record.size);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(record.address), record.size);
    DWORD ignored = 0;
    VirtualProtect(reinterpret_cast<void*>(record.address), record.size, old_protect, &ignored);
    record.saved = false;
    record.size = 0;
}

bool MakeRelativePatch(unsigned char opcode, std::uintptr_t source,
                       const void* destination, std::array<unsigned char, 5>& out) {
    const auto target = reinterpret_cast<std::uintptr_t>(destination);
    const auto displacement64 = static_cast<std::int64_t>(target) -
                                static_cast<std::int64_t>(source + 5u);
    if (displacement64 < std::numeric_limits<std::int32_t>::min() ||
        displacement64 > std::numeric_limits<std::int32_t>::max()) return false;
    const auto displacement = static_cast<std::int32_t>(displacement64);
    out[0] = opcode;
    std::memcpy(out.data() + 1, &displacement, sizeof(displacement));
    return true;
}

void LoadExpansionConfig() {
    if (g_config_loaded) return;
    g_config_loaded = true;

    wchar_t module_path[MAX_PATH * 4]{};
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&LoadExpansionConfig), &module);
    GetModuleFileNameW(module, module_path, static_cast<DWORD>(std::size(module_path)));
    std::wstring ini_path(module_path);
    const auto slash = ini_path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) ini_path.resize(slash + 1);
    ini_path += L"skill.ini";
    g_skill_ini_path = ini_path;

    g_expand_enabled = GetPrivateProfileIntW(
        L"ExpandObjID", L"Enable", 0, ini_path.c_str()) != 0;
    if (g_expand_enabled) {
        const UINT configured_count = GetPrivateProfileIntW(
            L"ExpandObjID", L"Count", 0, ini_path.c_str());
        const int count = static_cast<int>((std::min)(configured_count, 16u));
        for (int i = 1; i <= count; ++i) {
            wchar_t key[32]{}; wchar_t value[128]{};
            _snwprintf_s(key, _TRUNCATE, L"Range%d", i);
            GetPrivateProfileStringW(L"ExpandObjID", key, L"", value,
                                     static_cast<DWORD>(std::size(value)), ini_path.c_str());
            int first = 0, last = -1;
            if (swscanf_s(value, L"%d,%d", &first, &last) == 2 && first <= last) {
                g_configured_ranges.emplace_back(first, last);
            }
        }
    }
    g_skill_slot_offset_x = GetPrivateProfileIntW(
        L"ExpandObjID", L"SkillSlotOffsetX", 0, ini_path.c_str());
    g_skill_slot_offset_y = GetPrivateProfileIntW(
        L"ExpandObjID", L"SkillSlotOffsetY", 0, ini_path.c_str());
    g_skill_offset_refresh_tick = GetTickCount();
}

void RefreshSkillSlotOffsets() {
    LoadExpansionConfig();
    const DWORD now = GetTickCount();
    if (now - g_skill_offset_refresh_tick < 1000u) return;
    g_skill_offset_refresh_tick = now;
    g_skill_slot_offset_x = GetPrivateProfileIntW(
        L"ExpandObjID", L"SkillSlotOffsetX", 0, g_skill_ini_path.c_str());
    g_skill_slot_offset_y = GetPrivateProfileIntW(
        L"ExpandObjID", L"SkillSlotOffsetY", 0, g_skill_ini_path.c_str());
}

bool IsConfiguredObjectIndex(std::int32_t index) {
    for (const auto& range : g_configured_ranges) {
        if (index >= range.first && index <= range.second) return true;
    }
    return false;
}

extern "C" BOOL __stdcall ObjectIndexAllowed(std::int32_t index) {
    if (index >= 24100 && index < 24400) return TRUE;
    SharedObjectPathsLock lock;
    return g_object_paths.find(index) != g_object_paths.end() ? TRUE : FALSE;
}

extern "C" const wchar_t* __stdcall ObjectPathForIndex(std::int32_t index) {
    // Copy under the shared lock. Returning unordered_map::value.c_str() after
    // releasing the lock is unsafe because a concurrent registration may
    // replace the value and invalidate that pointer before the game consumes it.
    SharedObjectPathsLock lock;
    const auto it = g_object_paths.find(index);
    if (it == g_object_paths.end()) {
        g_object_path_result.clear();
        return nullptr;
    }
    g_object_path_result = it->second;
    return g_object_path_result.c_str();
}

#if defined(_MSC_VER) && defined(_M_IX86)
extern "C" __declspec(naked) void CaptureObjectManagerHook() {
    __asm {
        mov dword ptr [g_current_object_manager], ecx
        mov eax, kCaptureObjectOriginal
        jmp eax
    }
}

extern "C" __declspec(naked) void ObjectPathThunk() {
    __asm {
        xor eax, eax
        push dword ptr [ebp + 8]
        call ObjectPathForIndex
        mov esp, ebp
        pop ebp
        ret 4
    }
}

extern "C" __declspec(naked) void ConditionalRangeThunk() {
    __asm {
        push esi
        call IsConfiguredObjectIndex
        add esp, 4
        // The overwritten instruction is exactly: lea edx,[esi-0x5E89].
        // Recreate EDX after the C++ call so caller-saved EDX cannot be lost.
        lea edx, [esi - 05E89h]
        test al, al
        jz rejected
        push dword ptr [g_range_allowed_continuation]
        ret
    rejected:
        push dword ptr [g_range_rejected_continuation]
        ret
    }
}

extern "C" __declspec(naked) void AdditionalObjectThunk() {
    __asm {
        mov ecx, dword ptr [eax + 0ECh]
        cmp ecx, 4
        jl continue_original
        mov ecx, 3
    continue_original:
        jmp dword ptr [g_additional_continuation]
    }
}
#else
#error object_hooks.cpp requires 32-bit MSVC inline assembly
#endif

} // namespace

bool InstallObjectHooks() {
    LoadExpansionConfig();
    if (g_hooks_installed) return true;
    // Exact unpatched bytes from this client's DNF.exe. Refuse to modify a
    // different build or a site already owned by another plugin.
    if (!MatchesBytes(kCaptureObjectPatch, kExpectedCaptureBytes, 5) ||
        !MatchesBytes(kObjectAllowedPatch, kExpectedAllowedBytes, 5) ||
        !MatchesBytes(kObjectPathPatch, kExpectedPathBytes, 5) ||
        !MatchesBytes(kObjectBranchPatch, kExpectedBranchBytes, 2) ||
        !MatchesBytes(kAdditionalObjectPatch, kExpectedAdditionalBytes, 6) ||
        (g_expand_enabled && !g_configured_ranges.empty() &&
         !MatchesBytes(kConditionalRangePatch, kExpectedConditionalRangeBytes, 6))) {
        return false;
    }

    std::array<unsigned char, 5> capture{};
    std::array<unsigned char, 5> allowed{};
    std::array<unsigned char, 5> path{};
    std::array<unsigned char, 5> additional{};
    if (!MakeRelativePatch(0xE8, kCaptureObjectPatch,
                           reinterpret_cast<const void*>(&CaptureObjectManagerHook), capture) ||
        !MakeRelativePatch(0xE8, kObjectAllowedPatch,
                           reinterpret_cast<const void*>(&ObjectIndexAllowed), allowed) ||
        !MakeRelativePatch(0xE9, kObjectPathPatch,
                           reinterpret_cast<const void*>(&ObjectPathThunk), path) ||
        !MakeRelativePatch(0xE9, kAdditionalObjectPatch,
                           reinterpret_cast<const void*>(&AdditionalObjectThunk), additional)) {
        return false;
    }

    unsigned char conditional_range[6]{0x68, 0, 0, 0, 0, 0xC3};
    const auto conditional_target =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&ConditionalRangeThunk));
    std::memcpy(conditional_range + 1, &conditional_target, sizeof(conditional_target));

    const unsigned char branch_nops[2]{0x90, 0x90};
    if (!WritePatch(g_capture_patch, capture.data(), capture.size()) ||
        !WritePatch(g_allowed_patch, allowed.data(), allowed.size()) ||
        !WritePatch(g_path_patch, path.data(), path.size()) ||
        !WritePatch(g_branch_patch, branch_nops, sizeof(branch_nops)) ||
        (g_expand_enabled && !g_configured_ranges.empty() &&
         !WritePatch(g_range_patch, conditional_range, sizeof(conditional_range))) ||
        !WritePatch(g_additional_patch, additional.data(), additional.size())) {
        RemoveObjectHooks();
        return false;
    }
    g_hooks_installed = true;
    return true;
}

void RemoveObjectHooks() {
    RestorePatch(g_additional_patch);
    RestorePatch(g_range_patch);
    RestorePatch(g_branch_patch);
    RestorePatch(g_path_patch);
    RestorePatch(g_allowed_patch);
    RestorePatch(g_capture_patch);
    g_hooks_installed = false;
    g_current_object_manager = 0;
}

bool ObjectHooksInstalled() {
    return g_hooks_installed;
}

void ClearObjectPaths() {
    ExclusiveObjectPathsLock lock;
    g_object_paths.clear();
}

std::int32_t SkillSlotOffsetX() {
    RefreshSkillSlotOffsets();
    return g_skill_slot_offset_x;
}

std::int32_t SkillSlotOffsetY() {
    RefreshSkillSlotOffsets();
    return g_skill_slot_offset_y;
}

bool RegisterObjectPath(const wchar_t* object_name,
                        std::int32_t object_index,
                        const wchar_t* member_path) {
    if (!object_name || !member_path || !IsCommitted(kPushObject, 1)) return false;
    {
        ExclusiveObjectPathsLock lock;
        g_object_paths[object_index] = member_path;
    }
    using PushObjectFn = void(__cdecl*)(const wchar_t*, std::int32_t);
    reinterpret_cast<PushObjectFn>(kPushObject)(object_name, object_index);
    return true;
}

} // namespace dofext

#include "dof_extension/extension_api.hpp"
#include "host/game_squirrel_vm_adapter.hpp"

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dofext {
namespace {

// Current client proof: dofExtension_dump.dll calls these DNF.exe Squirrel APIs
// directly from 0x65120460, 0x65120900 and 0x651214C0..0x65121E50.
constexpr std::uintptr_t kSqPushString    = 0x01358A60;
constexpr std::uintptr_t kSqPushInteger   = 0x01358AD0;
constexpr std::uintptr_t kSqPushFloat     = 0x01358B60;
constexpr std::uintptr_t kSqPushRootTable = 0x01358C50;
constexpr std::uintptr_t kSqPushStack     = 0x01358C90;
constexpr std::uintptr_t kSqGetInteger    = 0x01358D70;
constexpr std::uintptr_t kSqGetFloat      = 0x01358DD0;
constexpr std::uintptr_t kSqGetBool       = 0x01358E30;
constexpr std::uintptr_t kSqGetString     = 0x01358E70;
constexpr std::uintptr_t kSqGetTop        = 0x01358FC0;
constexpr std::uintptr_t kSqPop           = 0x01358FF0;
constexpr std::uintptr_t kSqRemove        = 0x01359000;
constexpr std::uintptr_t kSqCall          = 0x01359280;
constexpr std::uintptr_t kSqReadClosure   = 0x01359460;
constexpr std::uintptr_t kSqGetInstanceUp = 0x0135A9C0;
constexpr std::uintptr_t kSqSetTop        = 0x0135AA40;
constexpr std::uintptr_t kSqNewSlot       = 0x0135AA80;
constexpr std::uintptr_t kSqGet           = 0x0135AE30;
constexpr std::uintptr_t kSqNewClosure    = 0x0135B850;

constexpr DWORD kExpectedDnfTimestamp = 0x51C3CCF7;
constexpr DWORD kExpectedDnfImageSize = 0x071A2000;

using FnGetInteger = int(DOFEXT_CDECL*)(VmHandle, int, std::int32_t*);
using FnGetFloat = int(DOFEXT_CDECL*)(VmHandle, int, float*);
using FnGetBool = int(DOFEXT_CDECL*)(VmHandle, int, std::int32_t*);
using FnGetString = int(DOFEXT_CDECL*)(VmHandle, int, const wchar_t**);
using FnGetInstanceUp = int(DOFEXT_CDECL*)(VmHandle, int, void**, void*);
using FnGetTop = int(DOFEXT_CDECL*)(VmHandle);
using FnSetTop = int(DOFEXT_CDECL*)(VmHandle, int);
using FnPushInteger = void(DOFEXT_CDECL*)(VmHandle, std::int32_t);
using FnPushFloat = void(DOFEXT_CDECL*)(VmHandle, float);
using FnPushString = void(DOFEXT_CDECL*)(VmHandle, const wchar_t*, int);
using FnPushRootTable = void(DOFEXT_CDECL*)(VmHandle);
using FnPushStack = void(DOFEXT_CDECL*)(VmHandle, int);
using FnPop = void(DOFEXT_CDECL*)(VmHandle);
using FnRemove = void(DOFEXT_CDECL*)(VmHandle, int);
using FnGet = int(DOFEXT_CDECL*)(VmHandle, int);
using FnCall = int(DOFEXT_CDECL*)(VmHandle, int, int, int);
using ReadClosureCallback = int(DOFEXT_CDECL*)(void*, void*, int);
using FnReadClosure = int(DOFEXT_CDECL*)(VmHandle, ReadClosureCallback, void*);
using FnNewClosure = void(DOFEXT_CDECL*)(VmHandle, NativeCallback, int);
using FnNewSlot = int(DOFEXT_CDECL*)(VmHandle, int, int);

template <typename T>
T At(std::uintptr_t address) {
    return reinterpret_cast<T>(address);
}

bool ValidateCurrentDnfImage() {
    const auto base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    if (reinterpret_cast<std::uintptr_t>(base) != 0x00400000 || !base) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != kExpectedDnfTimestamp ||
        nt->OptionalHeader.SizeOfImage != kExpectedDnfImageSize) return false;

    // Every recovered Squirrel entry in this exact DNF.exe starts with a normal
    // x86 frame prologue. Reject another client build instead of jumping to a
    // guessed address.
    const std::uintptr_t entries[] = {
        kSqPushString, kSqPushInteger, kSqPushFloat, kSqPushRootTable,
        kSqPushStack, kSqGetInteger, kSqGetFloat, kSqGetBool, kSqGetString,
        kSqGetTop, kSqPop, kSqRemove, kSqCall, kSqReadClosure,
        kSqGetInstanceUp, kSqSetTop,
        kSqNewSlot, kSqGet, kSqNewClosure,
    };
    for (const auto address : entries) {
        const auto* p = reinterpret_cast<const std::uint8_t*>(address);
        if (p[0] != 0x55 || p[1] != 0x8B || p[2] != 0xEC) return false;
    }
    return true;
}

bool Ready(VmHandle vm) {
    static const bool compatible = ValidateCurrentDnfImage();
    return compatible && vm != nullptr;
}

bool GameGetInteger(VmHandle vm, int index, std::int32_t* out) {
    return Ready(vm) && out && At<FnGetInteger>(kSqGetInteger)(vm, index, out) >= 0;
}

bool GameGetFloat(VmHandle vm, int index, float* out) {
    if (!Ready(vm) || !out) return false;
    if (At<FnGetFloat>(kSqGetFloat)(vm, index, out) >= 0) return true;
    std::int32_t integer = 0;
    if (!GameGetInteger(vm, index, &integer)) return false;
    *out = static_cast<float>(integer);
    return true;
}

bool GameGetBool(VmHandle vm, int index, bool* out) {
    if (!Ready(vm) || !out) return false;
    std::int32_t value = 0;
    if (At<FnGetBool>(kSqGetBool)(vm, index, &value) < 0 &&
        !GameGetInteger(vm, index, &value)) return false;
    *out = value != 0;
    return true;
}

bool GameGetString(VmHandle vm, int index, const wchar_t** out) {
    if (!Ready(vm) || !out) return false;
    *out = nullptr;
    return At<FnGetString>(kSqGetString)(vm, index, out) >= 0 && *out != nullptr;
}

bool SafeRead32(std::uintptr_t address, std::uint32_t& out) {
    if (!address) return false;
    __try {
        out = *reinterpret_cast<const volatile std::uint32_t*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = 0;
        return false;
    }
}

bool GetWrappedGameObject(VmHandle vm, int index, std::uintptr_t* out) {
    // Exact a4=0 path of current sub_651218C0. The modified VM stores each
    // SQObject as {type,value}; 0x0A008000 wraps a native game object and the
    // real pointer is stored at wrapper+0x20.
    if (!vm || !out) return false;
    *out = 0;
    const auto vm_address = reinterpret_cast<std::uintptr_t>(vm);
    std::uint32_t stack_storage = 0;
    std::uint32_t stack_base_or_top = 0;
    if (!SafeRead32(vm_address + 0x18u, stack_storage) || !stack_storage ||
        !SafeRead32(vm_address + (index < 0 ? 0x30u : 0x34u),
                    stack_base_or_top)) {
        return false;
    }
    const std::int64_t adjusted_base = index < 0
        ? static_cast<std::int64_t>(stack_base_or_top)
        : static_cast<std::int64_t>(stack_base_or_top) - 1;
    const std::int64_t slot_index = adjusted_base + index;
    if (slot_index < 0 || slot_index > 0x0FFFFFFFll) return false;
    const auto slot = static_cast<std::uintptr_t>(stack_storage) +
                      static_cast<std::uintptr_t>(slot_index) * 8u;
    std::uint32_t type = 0;
    std::uint32_t wrapper = 0;
    std::uint32_t object = 0;
    if (!SafeRead32(slot, type) || type != 0x0A008000u ||
        !SafeRead32(slot + 4u, wrapper) || !wrapper ||
        !SafeRead32(static_cast<std::uintptr_t>(wrapper) + 0x20u, object) ||
        !object) {
        return false;
    }
    *out = object;
    return true;
}

bool GameGetObject(VmHandle vm, int index, std::uintptr_t* out) {
    if (!Ready(vm) || !out) return false;
    if (GetWrappedGameObject(vm, index, out)) return true;
    void* value = nullptr;
    if (At<FnGetInstanceUp>(kSqGetInstanceUp)(vm, index, &value, nullptr) >= 0 && value) {
        *out = reinterpret_cast<std::uintptr_t>(value);
        return true;
    }
    std::int32_t integer = 0;
    if (!GameGetInteger(vm, index, &integer) || integer == 0) return false;
    *out = static_cast<std::uint32_t>(integer);
    return true;
}

int GameGetTop(VmHandle vm) {
    return Ready(vm) ? At<FnGetTop>(kSqGetTop)(vm) : 0;
}

void GamePushInteger(VmHandle vm, std::int32_t value) {
    if (Ready(vm)) At<FnPushInteger>(kSqPushInteger)(vm, value);
}

void GamePushFloat(VmHandle vm, float value) {
    if (Ready(vm)) At<FnPushFloat>(kSqPushFloat)(vm, value);
}

void GamePushString(VmHandle vm, const wchar_t* value) {
    if (Ready(vm)) At<FnPushString>(kSqPushString)(vm, value ? value : L"", -1);
}

void GamePushObject(VmHandle vm, std::uintptr_t value) {
    // No recovered callback currently uses VmApi::push_object. If one is added,
    // its exact SQ object representation must be proved before binding it.
    if (Ready(vm)) At<FnPushInteger>(kSqPushInteger)(vm, static_cast<std::int32_t>(value));
}

int GameRaiseError(VmHandle, const wchar_t*) {
    // The current plugin's clear callbacks use a normal failure return instead
    // of an unproved throw-error entry. Never jump to a guessed Squirrel API.
    return -1;
}

bool GameRegisterNative(VmHandle vm, const wchar_t* name,
                        NativeCallback callback, bool force) {
    if (!Ready(vm) || !name || !*name || !callback) return false;
    const int old_top = At<FnGetTop>(kSqGetTop)(vm);
    if (!force) {
        At<FnPushRootTable>(kSqPushRootTable)(vm);
        At<FnPushString>(kSqPushString)(vm, name, -1);
        const bool exists = At<FnGet>(kSqGet)(vm, -2) >= 0;
        At<FnSetTop>(kSqSetTop)(vm, old_top);
        if (exists) return true;
    }
    At<FnPushRootTable>(kSqPushRootTable)(vm);
    At<FnPushString>(kSqPushString)(vm, name, -1);
    At<FnNewClosure>(kSqNewClosure)(vm, callback, 0);
    const bool ok = At<FnNewSlot>(kSqNewSlot)(vm, -3, 0) >= 0;
    At<FnSetTop>(kSqSetTop)(vm, old_top);
    return ok;
}

struct ClosureReadCursor {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t offset = 0;
};

int DOFEXT_CDECL ReadSerializedClosure(void* raw_cursor, void* destination,
                                      int requested_bytes) {
    auto* cursor = static_cast<ClosureReadCursor*>(raw_cursor);
    if (!cursor || !destination || requested_bytes < 0 ||
        cursor->offset > cursor->size) return 0;
    const auto requested = static_cast<std::size_t>(requested_bytes);
    if (requested > cursor->size - cursor->offset) return 0;
    std::memcpy(destination, cursor->data + cursor->offset, requested);
    cursor->offset += requested;
    return requested_bytes;
}

bool GameCallNamed(VmHandle vm, const wchar_t* name,
                   int first_argument, int argument_count) {
    if (!Ready(vm) || !name || !*name || first_argument < 1 || argument_count < 0) {
        return false;
    }
    const int old_top = At<FnGetTop>(kSqGetTop)(vm);
    // Slot 1 is the root-table environment in every recovered callback. Resolve
    // directly against it instead of pushing a second root table; otherwise the
    // successful call leaves that extra table below the return value forever.
    At<FnPushString>(kSqPushString)(vm, name, -1);
    if (At<FnGet>(kSqGet)(vm, 1) < 0) {
        At<FnSetTop>(kSqSetTop)(vm, old_top);
        return false;
    }

    // Current sub_65120460(..., false) copies stack slot 1 as the root-table
    // environment before sub_6511D480 copies the user arguments.
    At<FnPushStack>(kSqPushStack)(vm, 1);
    for (int i = 0; i < argument_count; ++i) {
        At<FnPushStack>(kSqPushStack)(vm, first_argument + i);
    }
    if (At<FnCall>(kSqCall)(vm, argument_count + 1, 1, 1) < 0) {
        At<FnSetTop>(kSqSetTop)(vm, old_top);
        return false;
    }
    // sq_call pops only its parameters; the called closure remains immediately
    // below the return object. Remove that closure and leave exactly one result
    // above the callback's original stack, as required by return-count 1.
    At<FnRemove>(kSqRemove)(vm, -2);
    if (At<FnGetTop>(kSqGetTop)(vm) != old_top + 1) {
        At<FnSetTop>(kSqSetTop)(vm, old_top);
        return false;
    }
    return true;
}

} // namespace

bool TryUnwrapCurrentGameObject(VmHandle vm, int index, std::uintptr_t* out) {
    return GetWrappedGameObject(vm, index, out);
}

bool CurrentGameRootSlotExists(VmHandle vm, const wchar_t* name) {
    if (!Ready(vm) || !name || !*name) return false;
    const int old_top = At<FnGetTop>(kSqGetTop)(vm);
    At<FnPushRootTable>(kSqPushRootTable)(vm);
    At<FnPushString>(kSqPushString)(vm, name, -1);
    const bool exists = At<FnGet>(kSqGet)(vm, -2) >= 0;
    At<FnSetTop>(kSqSetTop)(vm, old_top);
    return exists;
}

bool InstallCurrentGameSerializedClosure(VmHandle vm, const wchar_t* name,
                                         const void* stream, int stream_size) {
    if (!Ready(vm) || !name || !*name || !stream || stream_size <= 0) return false;
    const int old_top = At<FnGetTop>(kSqGetTop)(vm);
    At<FnPushRootTable>(kSqPushRootTable)(vm);
    At<FnPushString>(kSqPushString)(vm, name, -1);

    ClosureReadCursor cursor{
        static_cast<const std::uint8_t*>(stream),
        static_cast<std::size_t>(stream_size), 0u};
    if (At<FnReadClosure>(kSqReadClosure)(
            vm, &ReadSerializedClosure, &cursor) < 0 ||
        cursor.offset != cursor.size) {
        At<FnSetTop>(kSqSetTop)(vm, old_top);
        return false;
    }
    const bool installed = At<FnNewSlot>(kSqNewSlot)(vm, -3, 0) >= 0;
    At<FnSetTop>(kSqSetTop)(vm, old_top);
    return installed;
}

bool CompileAndRunCurrentGameSource(VmHandle vm, const wchar_t* source,
                                    int source_length,
                                    const wchar_t* source_name) {
    if (!Ready(vm) || !source || source_length <= 0) return false;
    const int old_top = At<FnGetTop>(kSqGetTop)(vm);

    // Resolve the game's own root-table compilestring closure. That closure
    // invokes this exact DNF build's compiler, allocator and SQClosure factory.
    At<FnPushRootTable>(kSqPushRootTable)(vm);
    At<FnPushString>(kSqPushString)(vm, L"compilestring", -1);
    if (At<FnGet>(kSqGet)(vm, -2) < 0) {
        At<FnSetTop>(kSqSetTop)(vm, old_top);
        return false;
    }

    // compilestring(this, source, source_name) -> compiled closure
    At<FnPushRootTable>(kSqPushRootTable)(vm);
    At<FnPushString>(kSqPushString)(vm, source, source_length);
    At<FnPushString>(kSqPushString)(
        vm, source_name ? source_name : L"dofExtension_generated", -1);
    if (At<FnCall>(kSqCall)(vm, 3, 1, 1) < 0) {
        At<FnSetTop>(kSqSetTop)(vm, old_top);
        return false;
    }

    // Execute the returned script closure with the root table as environment.
    At<FnPushRootTable>(kSqPushRootTable)(vm);
    const bool ok = At<FnCall>(kSqCall)(vm, 1, 0, 1) >= 0;
    At<FnSetTop>(kSqSetTop)(vm, old_top);
    return ok;
}

VmApi MakeCurrentGameVmApi() {
    VmApi api{};
    api.get_int = &GameGetInteger;
    api.get_float = &GameGetFloat;
    api.get_bool = &GameGetBool;
    api.get_string = &GameGetString;
    api.get_object = &GameGetObject;
    api.get_top = &GameGetTop;
    api.push_int = &GamePushInteger;
    api.push_float = &GamePushFloat;
    api.push_string = &GamePushString;
    api.push_object = &GamePushObject;
    api.raise_error = &GameRaiseError;
    api.call_named = &GameCallNamed;
    api.register_native = &GameRegisterNative;
    return api;
}

} // namespace dofext

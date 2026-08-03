#include "core/callback_support.hpp"

#include <cstdint>
#include <cwchar>

namespace dofext {
namespace {

struct CurrentDispatchEntry {
    std::uint32_t id;
    std::int32_t function_suffix;
};

#include "current_chr_dispatch.inc"

bool LookupCurrentFunction(std::uint32_t id, std::int32_t& function_suffix) {
    // The current DLL performs this same linear search over 1803 entries at
    // 0x65146948. Preserve first-match behavior and do not reinterpret the ID.
    for (const auto& entry : kCurrentDispatchTable) {
        if (entry.id == id) {
            function_suffix = entry.function_suffix;
            return true;
        }
    }
    return false;
}

int CallCurrentFunctionByName(VmHandle vm, const wchar_t* function_name,
                              int first_argument) {
    if (!Context() || !Context()->vm.call_named || !Context()->vm.get_top ||
        !function_name) return PushInt(vm, 0);
    const int top = Context()->vm.get_top(vm);
    if (top < first_argument - 1) return PushInt(vm, 0);
    const int argument_count = top >= first_argument ? top - first_argument + 1 : 0;
    return Context()->vm.call_named(vm, function_name, first_argument,
                                    argument_count) ? 1 : PushInt(vm, 0);
}

int DispatchCurrentFunction(VmHandle vm, std::int32_t raw_id,
                            int first_argument) {
    std::int32_t function_suffix = 0;
    if (!LookupCurrentFunction(static_cast<std::uint32_t>(raw_id),
                               function_suffix)) {
        return PushInt(vm, 0);
    }
    wchar_t function_name[64]{};
    _snwprintf_s(function_name,
                   sizeof(function_name) / sizeof(function_name[0]), _TRUNCATE,
                 L"ChangQingFunctionChar%d", function_suffix);
    return CallCurrentFunctionByName(vm, function_name, first_argument);
}

} // namespace


int DOFEXT_CDECL cqx_callSquirrelChr(VmHandle vm) {
    // Current callback 0x65119F40: integer dispatch ID, arguments at index 4.
    std::int32_t function_id = 0;
    if (!GetInt(vm, 2, function_id)) return PushInt(vm, 0);
    return DispatchCurrentFunction(vm, function_id, 4);
}

int DOFEXT_CDECL cqx_callSquirrelFuncNew(VmHandle vm) {
    // Current callback 0x65119DE0: integer ID first, then string fallback.
    std::int32_t function_id = 0;
    if (GetInt(vm, 2, function_id)) {
        return DispatchCurrentFunction(vm, function_id, 3);
    }
    const wchar_t* function_name = nullptr;
    if (GetString(vm, 2, function_name)) {
        return CallCurrentFunctionByName(vm, function_name, 3);
    }
    return PushInt(vm, 0);
}

int DOFEXT_CDECL cqx_callSquirrelFunc(VmHandle vm) {
    // Current callback 0x65119E90: string name first, then integer ID fallback.
    const wchar_t* function_name = nullptr;
    if (GetString(vm, 2, function_name)) {
        return CallCurrentFunctionByName(vm, function_name, 3);
    }
    std::int32_t function_id = 0;
    if (GetInt(vm, 2, function_id)) {
        return DispatchCurrentFunction(vm, function_id, 3);
    }
    return PushInt(vm, 0);
}

int DOFEXT_CDECL CurrentRegisterObjectPath(VmHandle vm) {
    // Current callback 0x65119FB0, registered as cqx_pushObject.
    const wchar_t* object_name = nullptr;
    const wchar_t* member_path = nullptr;
    std::int32_t object_index = 0;
    if (!GetString(vm, 2, object_name) || !GetInt(vm, 3, object_index) ||
        !GetString(vm, 4, member_path)) return 0;
    if (Context() && Context()->host.push_object_by_name) {
        Context()->host.push_object_by_name(object_name, object_index, member_path);
    }
    return 0;
}

int DOFEXT_CDECL cqx_pushObject(VmHandle vm) {
    // Current callback 0x6511A230 treats arg2 as a UTF-16 pointer encoded in
    // an integer and pushes that string; null/failure returns integer zero.
    std::int32_t raw_pointer = 0;
    if (!GetInt(vm, 2, raw_pointer) || !raw_pointer) return PushInt(vm, 0);
    return PushString(vm, reinterpret_cast<const wchar_t*>(
        static_cast<std::uintptr_t>(static_cast<std::uint32_t>(raw_pointer))));
}

int DOFEXT_CDECL cqx_getInfoContent(VmHandle vm) {
    const wchar_t* key = nullptr;
    if (!GetString(vm, 2, key)) return Error(vm, L"cqx_getInfoContent: key expected");
    if (!Context()->host.lookup_info_content) return Error(vm, L"lookup_info_content is not bound");
    return PushInt(vm, Context()->host.lookup_info_content(key));
}

int DOFEXT_CDECL sqx_customName(VmHandle vm) {
    std::int32_t value = 0;
    if (!GetInt(vm, 2, value)) return Error(vm, L"sqx_customName: integer expected");
    return PushString(vm, reinterpret_cast<const wchar_t*>(
        static_cast<std::uintptr_t>(static_cast<std::uint32_t>(value))));
}

} // namespace dofext

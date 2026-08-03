#include "support/squirrel211_vm_adapter.hpp"

#include "squirrel.h"

#include <cstdint>

namespace dofext {
namespace {

HSQUIRRELVM AsVm(VmHandle vm) {
    return static_cast<HSQUIRRELVM>(vm);
}

bool VmGetInteger(VmHandle vm, int index, std::int32_t* out) {
    if (!vm || !out) return false;
    SQInteger value = 0;
    if (SQ_FAILED(sq_getinteger(AsVm(vm), index, &value))) return false;
    *out = static_cast<std::int32_t>(value);
    return true;
}

bool VmGetFloat(VmHandle vm, int index, float* out) {
    if (!vm || !out) return false;
    SQFloat value = 0;
    if (SQ_FAILED(sq_getfloat(AsVm(vm), index, &value))) return false;
    *out = static_cast<float>(value);
    return true;
}

bool VmGetBool(VmHandle vm, int index, bool* out) {
    if (!vm || !out) return false;
    SQBool value = SQFalse;
    if (SQ_FAILED(sq_getbool(AsVm(vm), index, &value))) return false;
    *out = value != SQFalse;
    return true;
}

bool VmGetString(VmHandle vm, int index, const wchar_t** out) {
    if (!vm || !out) return false;
    const SQChar* value = nullptr;
    if (SQ_FAILED(sq_getstring(AsVm(vm), index, &value))) return false;
    *out = reinterpret_cast<const wchar_t*>(value);
    return true;
}

bool VmGetObject(VmHandle vm, int index, std::uintptr_t* out) {
    if (!vm || !out) return false;
    SQUserPointer value = nullptr;
    if (SQ_FAILED(sq_getinstanceup(AsVm(vm), index, &value, nullptr))) return false;
    *out = reinterpret_cast<std::uintptr_t>(value);
    return true;
}

int VmGetTop(VmHandle vm) {
    return vm ? static_cast<int>(sq_gettop(AsVm(vm))) : 0;
}

void VmPushInteger(VmHandle vm, std::int32_t value) {
    sq_pushinteger(AsVm(vm), static_cast<SQInteger>(value));
}

void VmPushFloat(VmHandle vm, float value) {
    sq_pushfloat(AsVm(vm), static_cast<SQFloat>(value));
}

void VmPushBool(VmHandle vm, bool value) {
    sq_pushbool(AsVm(vm), value ? SQTrue : SQFalse);
}

void VmPushString(VmHandle vm, const wchar_t* value) {
    sq_pushstring(AsVm(vm), reinterpret_cast<const SQChar*>(value), -1);
}

void VmPushObject(VmHandle vm, std::uintptr_t value) {
    sq_pushuserpointer(AsVm(vm), reinterpret_cast<SQUserPointer>(value));
}

int VmRaiseError(VmHandle vm, const wchar_t* value) {
    return static_cast<int>(sq_throwerror(
        AsVm(vm), reinterpret_cast<const SQChar*>(value)));
}

bool VmRegisterNative(VmHandle vm, const wchar_t* name,
                      NativeCallback callback, bool force) {
    if (!vm || !name || !callback) return false;
    auto v = AsVm(vm);
    const SQInteger old_top = sq_gettop(v);
    if (!force) {
        sq_pushroottable(v);
        sq_pushstring(v, reinterpret_cast<const SQChar*>(name), -1);
        const bool exists = SQ_SUCCEEDED(sq_get(v, -2));
        sq_settop(v, old_top);
        if (exists) return true;
    }

    sq_pushroottable(v);
    sq_pushstring(v, reinterpret_cast<const SQChar*>(name), -1);
    sq_newclosure(v, reinterpret_cast<SQFUNCTION>(callback), 0);
    const bool ok = SQ_SUCCEEDED(sq_newslot(v, -3, SQFalse));
    sq_settop(v, old_top);
    return ok;
}

bool VmCallNamed(VmHandle vm, const wchar_t* name,
                 int first_argument, int argument_count) {
    if (!vm || !name || first_argument < 1 || argument_count < 0) return false;
    auto v = AsVm(vm);
    const SQInteger old_top = sq_gettop(v);

    HSQOBJECT arguments[32]{};
    if (argument_count > static_cast<int>(sizeof(arguments) / sizeof(arguments[0]))) {
        return false;
    }
    for (int i = 0; i < argument_count; ++i) {
        if (SQ_FAILED(sq_getstackobj(v, first_argument + i, &arguments[i]))) return false;
    }

    sq_pushroottable(v);
    sq_pushstring(v, reinterpret_cast<const SQChar*>(name), -1);
    if (SQ_FAILED(sq_get(v, -2))) {
        sq_settop(v, old_top);
        return false;
    }

    // Native/root functions receive the root table as their environment.
    sq_pushroottable(v);
    for (int i = 0; i < argument_count; ++i) {
        sq_pushobject(v, arguments[i]);
    }

    const SQRESULT call_result = sq_call(
        v, static_cast<SQInteger>(argument_count + 1), SQTrue, SQTrue);
    if (SQ_FAILED(call_result)) {
        sq_settop(v, old_top);
        return false;
    }

    HSQOBJECT returned{};
    sq_resetobject(&returned);
    if (SQ_FAILED(sq_getstackobj(v, -1, &returned))) {
        sq_settop(v, old_top);
        return false;
    }
    sq_addref(v, &returned);
    sq_settop(v, old_top);
    sq_pushobject(v, returned);
    sq_release(v, &returned);
    return true;
}

} // namespace

VmApi MakeSquirrel211VmApi() {
    static_assert(sizeof(SQInteger) == 4, "client uses 32-bit SQInteger");
    static_assert(sizeof(SQFloat) == 4, "client uses 32-bit SQFloat");
    static_assert(sizeof(SQChar) == sizeof(wchar_t), "client uses Unicode Squirrel");

    VmApi api{};
    api.get_int = &VmGetInteger;
    api.get_float = &VmGetFloat;
    api.get_bool = &VmGetBool;
    api.get_string = &VmGetString;
    api.get_object = &VmGetObject;
    api.get_top = &VmGetTop;
    api.push_int = &VmPushInteger;
    api.push_float = &VmPushFloat;

    api.push_string = &VmPushString;
    api.push_object = &VmPushObject;
    api.raise_error = &VmRaiseError;
    api.register_native = &VmRegisterNative;
    api.call_named = &VmCallNamed;
    return api;
}

} // namespace dofext

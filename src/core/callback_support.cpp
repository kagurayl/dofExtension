#include "core/callback_support.hpp"

namespace dofext {
namespace {

ExtensionContext* g_context = nullptr;

} // namespace

ExtensionContext* Context() {
    return g_context;
}

int Error(VmHandle vm, const wchar_t* message) {
    // Current 32 callbacks use a soft Squirrel failure: push integer zero and
    // return one value. They do not abort the caller with sq_throwerror.
    (void)message;
    if (g_context && g_context->vm.push_int) {
        g_context->vm.push_int(vm, 0);
        return 1;
    }
    return 0;
}

bool GetInt(VmHandle vm, int index, std::int32_t& value) {
    return g_context && g_context->vm.get_int &&
           g_context->vm.get_int(vm, index, &value);
}

bool GetFloat(VmHandle vm, int index, float& value) {
    return g_context && g_context->vm.get_float &&
           g_context->vm.get_float(vm, index, &value);
}

bool GetBool(VmHandle vm, int index, bool& value) {
    return g_context && g_context->vm.get_bool &&
           g_context->vm.get_bool(vm, index, &value);
}

bool GetString(VmHandle vm, int index, const wchar_t*& value) {
    return g_context && g_context->vm.get_string &&
           g_context->vm.get_string(vm, index, &value);
}

bool GetObjectArgument(VmHandle vm, int index, std::uintptr_t& value) {
    return g_context && g_context->vm.get_object &&
           g_context->vm.get_object(vm, index, &value);
}

int PushInt(VmHandle vm, std::int32_t value) {
    if (!g_context || !g_context->vm.push_int) return Error(vm, L"push_int is not bound");
    g_context->vm.push_int(vm, value);
    return 1;
}

int PushFloat(VmHandle vm, float value) {
    if (!g_context || !g_context->vm.push_float) return Error(vm, L"push_float is not bound");
    g_context->vm.push_float(vm, value);
    return 1;
}

int PushString(VmHandle vm, const wchar_t* value) {
    if (!g_context || !g_context->vm.push_string) return Error(vm, L"push_string is not bound");
    g_context->vm.push_string(vm, value ? value : L"");
    return 1;
}

int PushObject(VmHandle vm, std::uintptr_t value) {
    if (!g_context || !g_context->vm.push_object) return Error(vm, L"push_object is not bound");
    g_context->vm.push_object(vm, value);
    return 1;
}

void SetExtensionContext(ExtensionContext* context) {
    g_context = context;
}

} // namespace dofext

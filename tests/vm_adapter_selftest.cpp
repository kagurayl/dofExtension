#include "dof_extension/extension_api.hpp"
#include "support/squirrel211_vm_adapter.hpp"
#include <squirrel.h>
#include <cstdio>
#include <cstdint>

struct CurrentDispatchEntry {
    std::uint32_t id;
    std::int32_t function_suffix;
};
#include "current_chr_dispatch.inc"

static SQInteger Target(HSQUIRRELVM vm) {
    SQInteger a = 0, b = 0;
    if (SQ_FAILED(sq_getinteger(vm, 2, &a)) ||
        SQ_FAILED(sq_getinteger(vm, 3, &b))) return SQ_ERROR;
    sq_pushinteger(vm, a * 100 + b);
    return 1;
}

int main() {
    constexpr std::uint32_t wanted_id = 0x1D027956u;
    std::int32_t suffix = 0;
    std::size_t matches = 0;
    for (const auto& entry : kCurrentDispatchTable) {
        if (entry.id == wanted_id) {
            suffix = entry.function_suffix;
            ++matches;
        }
    }
    constexpr std::size_t dispatch_count =
        sizeof(kCurrentDispatchTable) / sizeof(kCurrentDispatchTable[0]);
    if (matches != 1 || suffix != 486701398 || dispatch_count != 1803) return 1;

    wchar_t function_name[64]{};
    _snwprintf_s(function_name, 64, _TRUNCATE,
                 L"ChangQingFunctionChar%d", suffix);

    HSQUIRRELVM vm = sq_open(128);
    if (!vm) return 2;
    sq_pushroottable(vm);
    sq_pushstring(vm, function_name, -1);
    sq_newclosure(vm, &Target, 0);
    if (SQ_FAILED(sq_newslot(vm, -3, SQFalse))) return 3;
    sq_settop(vm, 0);

    // Current cqx_callSquirrelFuncNew/cqx_callSquirrelFunc use argument slot 3:
    // slot1=this/root, slot2=dispatch ID, slots3..N=forwarded arguments.
    sq_pushnull(vm);
    sq_pushinteger(vm, static_cast<SQInteger>(wanted_id));
    sq_pushinteger(vm, 7);
    sq_pushinteger(vm, 9);
    auto api = dofext::MakeSquirrel211VmApi();
    if (!api.call_named(vm, function_name, 3, 2)) return 4;
    SQInteger result = 0;
    if (SQ_FAILED(sq_getinteger(vm, -1, &result))) return 5;
    std::printf("dispatch_count=%d id=0x%08X suffix=%d call_ok=1 result=%d top=%d\n",
                static_cast<int>(dispatch_count), wanted_id,
                suffix, static_cast<int>(result), static_cast<int>(sq_gettop(vm)));
    sq_close(vm);
    return result == 709 ? 0 : 6;
}

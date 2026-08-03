#include "core/callback_support.hpp"
#include "core/native_callbacks.hpp"

#include <array>

namespace dofext {
namespace {

struct Binding {
    const wchar_t* name;
    NativeCallback callback;
    bool force = false;
};

} // namespace

bool RegisterSquirrelExtensions(VmHandle vm) {
    if (!Context() || !Context()->vm.register_native) return false;
    static constexpr std::array<Binding, 32> bindings{{
        {L"cqx_callSquirrelFuncNew", cqx_callSquirrelFuncNew, true},
        {L"cqx_callSquirrelFunc", cqx_callSquirrelFunc},
        {L"cqx_callSquirrelChr", cqx_callSquirrelChr},
        {L"cqx_pushObject", CurrentRegisterObjectPath},
        {L"sqx_customName", cqx_pushObject},
        {L"cqx_getScreenX", cqx_getScreenX},
        {L"cqx_readValue", cqx_readValue},
        {L"cqx_getSkillCurrentCool", cqx_getSkillCurrentCool},
        {L"cqx_DrawSkillIcon", cqx_DrawSkillIcon},
        {L"cqx_DrawItemSlot", cqx_DrawItemSlot},
        {L"cqx_drawText", cqx_drawText},
        {L"cqx_drawTips", cqx_drawTips},
        {L"cqx_drawImage", cqx_drawImage},
        {L"cqx_packetWriteHeader", cqx_packetWriteHeader},
        {L"cqx_PacketWriteData", cqx_PacketWriteData},
        {L"cqx_SendPacket", cqx_SendPacket},
        {L"cqx_getStaticData", cqx_getStaticData},
        {L"cqx_getLeveData", cqx_getLeveData},
        {L"cqx_setStaticData", cqx_setStaticData},
        {L"cqx_setLevelData", cqx_setLevelData},
        {L"cqx_saveDataSkillFile", cqx_saveDataSkillFile},
        {L"cqx_getinfoFromData", cqx_getinfoFromData},
        {L"cqx_GetChrName", cqx_GetChrName},
        {L"cqx_getObjectAnyInfo", cqx_getObjectAnyInfo},
        {L"cqx_getObjectInfo", cqx_getObjectInfo},
        {L"cqx_setObjectData", cqx_setObjectData},
        {L"cqx_getObjectInfo2nd", cqx_getObjectInfo2nd},
        {L"cqx_setObjectInfo2nd", cqx_setObjectInfo2nd},
        {L"cqx_sendNotice", cqx_sendNotice},
        {L"cqx_exitDungeon", cqx_exitDungeon},
        {L"cqx_startDungeon", cqx_startDungeon},
        {L"cqx_getItemSlotIndex", cqx_getItemSlotIndex},
    }};

    for (const auto& binding : bindings) {
        if (!Context()->vm.register_native(vm, binding.name, binding.callback,
                                           binding.force)) {
            return false;
        }
    }
    return true;
}

} // namespace dofext

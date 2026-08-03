#pragma once

#include "dof_extension/extension_api.hpp"

namespace dofext {

int DOFEXT_CDECL cqx_callSquirrelFuncNew(VmHandle vm);
int DOFEXT_CDECL cqx_callSquirrelFunc(VmHandle vm);
int DOFEXT_CDECL cqx_callSquirrelChr(VmHandle vm);
int DOFEXT_CDECL CurrentRegisterObjectPath(VmHandle vm);
int DOFEXT_CDECL cqx_pushObject(VmHandle vm);
int DOFEXT_CDECL cqx_getInfoContent(VmHandle vm);
int DOFEXT_CDECL sqx_customName(VmHandle vm);
int DOFEXT_CDECL cqx_getScreenX(VmHandle vm);
int DOFEXT_CDECL cqx_readValue(VmHandle vm);
int DOFEXT_CDECL cqx_getSkillCurrentCool(VmHandle vm);
int DOFEXT_CDECL cqx_DrawSkillIcon(VmHandle vm);
int DOFEXT_CDECL cqx_DrawItemSlot(VmHandle vm);
int DOFEXT_CDECL cqx_drawText(VmHandle vm);
int DOFEXT_CDECL cqx_drawTips(VmHandle vm);
int DOFEXT_CDECL cqx_drawImage(VmHandle vm);
int DOFEXT_CDECL cqx_packetWriteHeader(VmHandle vm);
int DOFEXT_CDECL cqx_PacketWriteData(VmHandle vm);
int DOFEXT_CDECL cqx_SendPacket(VmHandle vm);
int DOFEXT_CDECL cqx_getStaticData(VmHandle vm);
int DOFEXT_CDECL cqx_getLeveData(VmHandle vm);
int DOFEXT_CDECL cqx_setStaticData(VmHandle vm);
int DOFEXT_CDECL cqx_setLevelData(VmHandle vm);
int DOFEXT_CDECL cqx_saveDataSkillFile(VmHandle vm);
int DOFEXT_CDECL cqx_getinfoFromData(VmHandle vm);
int DOFEXT_CDECL cqx_GetChrName(VmHandle vm);
int DOFEXT_CDECL cqx_getObjectAnyInfo(VmHandle vm);
int DOFEXT_CDECL cqx_getObjectInfo(VmHandle vm);
int DOFEXT_CDECL cqx_setObjectData(VmHandle vm);
int DOFEXT_CDECL cqx_getObjectInfo2nd(VmHandle vm);
int DOFEXT_CDECL cqx_setObjectInfo2nd(VmHandle vm);
int DOFEXT_CDECL cqx_sendNotice(VmHandle vm);
int DOFEXT_CDECL cqx_exitDungeon(VmHandle vm);
int DOFEXT_CDECL cqx_startDungeon(VmHandle vm);
int DOFEXT_CDECL cqx_GetGameState(VmHandle vm);
int DOFEXT_CDECL cqx_GetTownIndex(VmHandle vm);
int DOFEXT_CDECL cqx_GetAreaIndex(VmHandle vm);
int DOFEXT_CDECL cqx_getItemSlotIndex(VmHandle vm);

} // namespace dofext

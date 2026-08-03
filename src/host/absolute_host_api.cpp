#include "host/absolute_host_api.hpp"
#include "host/absolute_host_functions.hpp"

namespace dofext {

HostApi MakeAbsoluteHostApi() {
    using namespace host_detail;
    HostApi api{};
    api.push_object_by_name = &AbsolutePushObjectByName;
    api.resolve_path = &AbsoluteResolvePath;
    api.decode_i32 = &AbsoluteDecode;
    api.encode_i32 = &AbsoluteEncode;
    api.get_screen_x = &AbsoluteScreenX;
    api.get_skill_cooldown = &AbsoluteSkillCooldown;
    api.draw_skill_icon = &AbsoluteDrawSkillIcon;
    api.draw_item_slot = &AbsoluteDrawItemSlot;
    api.draw_text = &AbsoluteDrawText;
    api.draw_tips = &AbsoluteDrawTips;
    api.draw_image = &AbsoluteDrawImage;
    api.send_notice = &AbsoluteNotice;
    api.packet_begin = &AbsolutePacketBegin;
    api.packet_write = &AbsolutePacketWrite;
    api.packet_send = &AbsolutePacketSend;
    api.lookup_info_content = &AbsoluteLookupInfo;
    api.get_info_from_data = &AbsoluteGetInfoFromData;
    api.get_character_name = &AbsoluteCharacterName;
    api.get_game_state = &AbsoluteGameState;
    api.get_town_index = &AbsoluteTownIndex;
    api.get_area_index = &AbsoluteArea;
    api.get_item_slot_index = &AbsoluteItemSlotIndex;
    api.save_skill_data = &AbsoluteSaveSkillData;
    return api;
}

} // namespace dofext

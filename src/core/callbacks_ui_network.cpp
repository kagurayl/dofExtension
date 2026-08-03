#include "core/callback_support.hpp"
#include "core/native_callbacks.hpp"

namespace dofext {

int DOFEXT_CDECL cqx_getScreenX(VmHandle vm) {
    if (!Context() || !Context()->host.get_screen_x) return Error(vm, L"get_screen_x is not bound");
    return PushInt(vm, Context()->host.get_screen_x());
}


int DOFEXT_CDECL cqx_getSkillCurrentCool(VmHandle vm) {
    std::uintptr_t skill = 0;
    if (!GetObjectArgument(vm, 2, skill)) return Error(vm, L"failed to get object");
    if (!Context()->host.get_skill_cooldown) return Error(vm, L"get_skill_cooldown is not bound");
    return PushInt(vm, Context()->host.get_skill_cooldown(skill));
}

int DOFEXT_CDECL cqx_DrawSkillIcon(VmHandle vm) {
    DrawSkillArgs args{};
    if (!GetObjectArgument(vm, 2, args.skill) || !GetInt(vm, 3, args.x) ||
        !GetInt(vm, 4, args.y) || !GetFloat(vm, 5, args.scale_x) ||
        !GetFloat(vm, 6, args.scale_y) || !GetInt(vm, 7, args.alpha)) {
        return Error(vm, L"cqx_DrawSkillIcon: invalid arguments");
    }
    if (!Context()->host.draw_skill_icon) return Error(vm, L"draw_skill_icon is not bound");
    Context()->host.draw_skill_icon(args);
    return 0;
}

int DOFEXT_CDECL cqx_DrawItemSlot(VmHandle vm) {
    std::int32_t slot = 0, x = 0, y = 0, flags = 0;
    if (!GetInt(vm, 2, slot) || !GetInt(vm, 3, x) || !GetInt(vm, 4, y) || !GetInt(vm, 5, flags)) {
        return Error(vm, L"cqx_DrawItemSlot: invalid arguments");
    }
    if (!Context()->host.draw_item_slot) return Error(vm, L"draw_item_slot is not bound");
    Context()->host.draw_item_slot(slot, x, y, flags);
    return 0;
}

int DOFEXT_CDECL cqx_drawText(VmHandle vm) {
    std::int32_t x = 0, y = 0, color = 0;
    const wchar_t* text = nullptr;
    if (!GetInt(vm, 2, x) || !GetInt(vm, 3, y) || !GetInt(vm, 4, color) || !GetString(vm, 5, text)) {
        return Error(vm, L"cqx_drawText: invalid arguments");
    }
    if (!Context()->host.draw_text) return Error(vm, L"draw_text is not bound");
    Context()->host.draw_text(x, y, color, text);
    return 0;
}

int DOFEXT_CDECL cqx_drawTips(VmHandle vm) {
    std::int32_t x = 0, y = 0, width = 0, style = 0, arg6 = 0, arg7 = 0;
    const wchar_t* text = nullptr;
    if (!GetInt(vm, 2, x) || !GetInt(vm, 3, y) || !GetInt(vm, 4, width) ||
        !GetInt(vm, 5, style) || !GetString(vm, 6, text) ||
        !GetInt(vm, 7, arg6) || !GetInt(vm, 8, arg7)) {
        return Error(vm, L"cqx_drawTips: invalid arguments");
    }
    if (!Context()->host.draw_tips) return Error(vm, L"draw_tips is not bound");
    Context()->host.draw_tips(x, y, width, style, text, arg6, arg7);
    return 0;
}

int DOFEXT_CDECL cqx_drawImage(VmHandle vm) {
    DrawImageArgs args{};
    if (!GetString(vm, 2, args.package) || !GetInt(vm, 3, args.image_index) ||
        !GetInt(vm, 4, args.x) || !GetInt(vm, 5, args.y) ||
        !GetInt(vm, 6, args.alpha) || !GetFloat(vm, 7, args.scale_x) ||
        !GetFloat(vm, 8, args.scale_y)) {
        return Error(vm, L"cqx_drawImage: invalid arguments");
    }
    if (!Context()->host.draw_image) return Error(vm, L"draw_image is not bound");
    if (!Context()->host.draw_image(args) && Context()->host.draw_tips) {
        Context()->host.draw_tips(args.x, args.y, -1, 2, L"load img error", 0, 0);
    }
    return 0;
}

int DOFEXT_CDECL cqx_packetWriteHeader(VmHandle vm) {
    std::int32_t message_id = 0;
    if (!GetInt(vm, 2, message_id)) return Error(vm, L"cqx_packetWriteHeader: message id expected");
    if (!Context()->host.packet_begin) return Error(vm, L"packet_begin is not bound");
    Context()->host.packet_begin(message_id);
    return 0;
}

int DOFEXT_CDECL cqx_PacketWriteData(VmHandle vm) {
    std::int32_t value = 0, width_type = 4;
    if (!GetInt(vm, 2, value)) return PushInt(vm, 0);
    GetInt(vm, 3, width_type); // optional in 0x6511AE70
    if (!Context()->host.packet_write) return Error(vm, L"packet_write is not bound");
    Context()->host.packet_write(value, width_type);
    return 0;
}

int DOFEXT_CDECL cqx_SendPacket(VmHandle vm) {
    if (!Context() || !Context()->host.packet_send) return Error(vm, L"packet_send is not bound");
    Context()->host.packet_send();
    return 0;
}

int DOFEXT_CDECL cqx_sendNotice(VmHandle vm) {
    const wchar_t* text = nullptr;
    std::int32_t color = 0, channel = 0;
    if (!GetString(vm, 2, text) || !GetInt(vm, 3, color) || !GetInt(vm, 4, channel)) {
        return Error(vm, L"cqx_sendNotice: invalid arguments");
    }
    if (!Context()->host.send_notice) return Error(vm, L"send_notice is not bound");
    Context()->host.send_notice(text, color, channel);
    return PushInt(vm, 0);
}

int DOFEXT_CDECL cqx_exitDungeon(VmHandle vm) {
    if (!Context() || !Context()->host.packet_begin || !Context()->host.packet_send) {
        return Error(vm, L"packet API is not bound");
    }
    Context()->host.packet_begin(45);
    Context()->host.packet_send();
    return 0;
}

int DOFEXT_CDECL cqx_startDungeon(VmHandle vm) {
    std::int32_t dungeon_id = 0, first_flag = 0, second_flag = 0;
    if (!GetInt(vm, 2, dungeon_id) || !GetInt(vm, 3, first_flag)) {
        return PushInt(vm, 0);
    }
    GetInt(vm, 4, second_flag); // optional in 0x6511AF40
    if (!Context()->host.packet_begin || !Context()->host.packet_write || !Context()->host.packet_send) {
        return Error(vm, L"packet API is not bound");
    }
    Context()->host.packet_begin(15);
    Context()->host.packet_send();
    Context()->host.packet_begin(16);
    Context()->host.packet_write(dungeon_id, 2); // 0x1128580: WORD
    Context()->host.packet_write(first_flag & 0xFF, 1); // 0x1128550: BYTE
    Context()->host.packet_write(second_flag & 0xFF, 1);
    Context()->host.packet_write(0, 1);
    Context()->host.packet_write(0, 1);
    Context()->host.packet_send();
    return 0;
}

} // namespace dofext

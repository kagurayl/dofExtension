#include "host/absolute_host_functions.hpp"
#include "host/host_addresses.hpp"

namespace dofext::host_detail {

void AbsoluteDrawSkillIcon(const DrawSkillArgs& a) {
    using Fn = void(__thiscall*)(void*, std::uintptr_t, std::int32_t, std::int32_t,
                                 std::int32_t, std::int32_t, float, float,
                                 std::int32_t);
    auto character = ReadAbs<std::uintptr_t>(address::kCharacterGlobal);
    reinterpret_cast<Fn>(address::kDrawSkillIcon)(
        reinterpret_cast<void*>(a.skill), character, a.x, a.y, 0, 1,
        a.scale_x, a.scale_y, a.alpha);
}

void AbsoluteDrawItemSlot(std::int32_t slot_id, std::int32_t x,
                          std::int32_t y, std::int32_t flags) {
    using ResolveFn = std::uintptr_t(__cdecl*)(std::int32_t);
    using DrawFn = void(__cdecl*)(std::int32_t, std::int32_t, std::int32_t,
                                  std::int32_t, std::int32_t, std::int32_t,
                                  std::int32_t);
    const auto slot = reinterpret_cast<ResolveFn>(address::kResolveItemSlot)(slot_id);
    reinterpret_cast<DrawFn>(address::kDrawItemSlot)(
        y, flags, static_cast<std::int32_t>(slot), x, 0, 0, 0);
}

void AbsoluteDrawText(std::int32_t x, std::int32_t y,
                      std::int32_t color, const wchar_t* text) {
    auto renderer = ReadAbs<void*>(address::kRendererGlobal);
    using BeginFn = void(__thiscall*)(void*, std::uintptr_t);
    using DrawFn = void(__thiscall*)(void*, std::int32_t, std::int32_t,
                                     std::uint32_t, const wchar_t*);
    using EndFn = void(__thiscall*)(void*);
    reinterpret_cast<BeginFn>(address::kTextBegin)(renderer, address::kTextContextGlobal);
    reinterpret_cast<DrawFn>(address::kTextDraw)(renderer, x, y, color, text);
    reinterpret_cast<EndFn>(address::kTextEnd)(renderer);
}

void AbsoluteDrawTips(std::int32_t x, std::int32_t y,
                      std::int32_t width, std::int32_t style,
                      const wchar_t* text, std::int32_t arg6,
                      std::int32_t arg7) {
    using Fn = void(__cdecl*)(std::int32_t, std::int32_t, std::int32_t,
                              std::uint32_t, std::int32_t, const wchar_t*,
                              std::int32_t, std::int32_t, std::int32_t,
                              std::int32_t, std::int32_t);
    reinterpret_cast<Fn>(address::kDrawTips)(
        x, y, width, 0xFF000000u, style, text,
        arg6, 0, arg7, 0, 1);
}

bool AbsoluteDrawImage(const DrawImageArgs& a) {
    using PackageFn = std::uintptr_t(__fastcall*)(void*, void*, const wchar_t*);
    using ImageFn = std::uintptr_t(__thiscall*)(void*, std::int32_t);
    using DrawFn = void(__thiscall*)(void*, std::int32_t, std::int32_t,
                                     std::uintptr_t, float, float,
                                     std::int32_t, std::uint32_t, float, float);
    auto manager = ReadAbs<void*>(address::kImageManagerGlobal);
    const auto pack = reinterpret_cast<PackageFn>(address::kImagePackageLookup)(
        manager, nullptr, a.package);
    if (!pack) return false;
    const auto bitmap = reinterpret_cast<ImageFn>(address::kImageLookup)(
        reinterpret_cast<void*>(pack), a.image_index);
    auto renderer = ReadAbs<void*>(address::kRendererGlobal);
    const auto global_scale = ReadAbs<float>(address::kScaleGlobal);
    reinterpret_cast<DrawFn>(address::kImageDraw)(
        renderer, a.x, a.y, bitmap, a.scale_x, a.scale_y, 0,
        (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a.alpha)) << 24) |
            0x00FFFFFFu,
        global_scale, global_scale);
    return true;
}

void AbsoluteNotice(const wchar_t* text, std::int32_t type, std::int32_t channel) {
    auto root = ReadAbs<std::uintptr_t>(address::kNoticeContextGlobal);
    auto context = static_cast<std::int32_t>(root) == -64
        ? nullptr : ReadAbs<void*>(root + 0x40u);
    using Fn = void(__fastcall*)(void*, void*, const wchar_t*, std::int32_t,
                                 std::int32_t, std::int32_t, std::int32_t,
                                 std::int32_t);
    reinterpret_cast<Fn>(address::kSendNotice)(context, nullptr, text, type,
                                               channel, 0, 0, 0);
}

void* PacketWriter() {
    return ReadAbs<void*>(address::kPacketWriterGlobal);
}

void AbsolutePacketBegin(std::int32_t message_id) {
    using Fn = void(__fastcall*)(void*, void*, std::int32_t);
    reinterpret_cast<Fn>(address::kPacketBegin)(PacketWriter(), nullptr, message_id);
}

void AbsolutePacketWrite(std::int32_t value, std::int32_t width_type) {
    using Fn32 = void(__fastcall*)(void*, void*, std::int32_t);
    using Fn64 = void(__fastcall*)(void*, void*, std::int32_t, std::int32_t);
    switch (width_type) {
    case 1:
        reinterpret_cast<Fn32>(address::kPacketWriteByte)(PacketWriter(), nullptr, value);
        break;
    case 2:
        reinterpret_cast<Fn32>(address::kPacketWriteWord)(PacketWriter(), nullptr, value);
        break;
    case 3:
        reinterpret_cast<Fn32>(address::kPacketWriteDword)(PacketWriter(), nullptr, value);
        break;
    default:
        reinterpret_cast<Fn64>(address::kPacketWriteQword)(PacketWriter(), nullptr,
                                                           value, value >> 31);
        break;
    }
}

void AbsolutePacketSend() {
    using Fn = void(__fastcall*)(void*, void*);
    reinterpret_cast<Fn>(address::kPacketSend)(PacketWriter(), nullptr);
}

} // namespace dofext::host_detail

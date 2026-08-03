#pragma once

#include <cstddef>
#include <cstdint>

namespace dofext {

#if defined(_MSC_VER) && defined(_M_IX86)
#define DOFEXT_CDECL __cdecl
#elif defined(_MSC_VER)
#error "dofExtension recovered source requires MSVC x86 (_M_IX86)"
#elif defined(__i386__)
#define DOFEXT_CDECL __attribute__((cdecl))
#else
#error "dofExtension recovered source requires a 32-bit x86 compiler"
#endif

using VmHandle = void*;
using NativeCallback = int(DOFEXT_CDECL*)(VmHandle);

// Operations needed by the recovered callbacks. Production binds these to the
// current game's Squirrel VM; tests bind them to stock Squirrel 2.1.
struct VmApi {
    bool (*get_int)(VmHandle vm, int index, std::int32_t* out) = nullptr;
    bool (*get_float)(VmHandle vm, int index, float* out) = nullptr;
    bool (*get_bool)(VmHandle vm, int index, bool* out) = nullptr;
    bool (*get_string)(VmHandle vm, int index, const wchar_t** out) = nullptr;
    bool (*get_object)(VmHandle vm, int index, std::uintptr_t* out) = nullptr;
    int (*get_top)(VmHandle vm) = nullptr;

    void (*push_int)(VmHandle vm, std::int32_t value) = nullptr;
    void (*push_float)(VmHandle vm, float value) = nullptr;
    void (*push_string)(VmHandle vm, const wchar_t* value) = nullptr;
    void (*push_object)(VmHandle vm, std::uintptr_t value) = nullptr;
    int (*raise_error)(VmHandle vm, const wchar_t* message) = nullptr;

    bool (*call_named)(VmHandle vm, const wchar_t* function_name,
                       int first_argument, int argument_count) = nullptr;
    bool (*register_native)(VmHandle vm, const wchar_t* name,
                            NativeCallback callback, bool force) = nullptr;
};

struct DrawImageArgs {
    const wchar_t* package = nullptr;
    std::int32_t image_index = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t alpha = 0;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
};

struct DrawSkillArgs {
    std::uintptr_t skill = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    std::int32_t alpha = 255;
};

// Host-facing operations that could not be made standalone without the exact
// game classes. Members correspond to behavior recovered from the callbacks;
// exact ABI details remain the responsibility of the version-specific adapter.
struct HostApi {
    bool (*push_object_by_name)(const wchar_t* object_name,
                                std::int32_t object_index,
                                const wchar_t* member_path) = nullptr;
    std::uintptr_t (*resolve_path)(std::uintptr_t base,
                                   const wchar_t* path) = nullptr;
    std::int32_t (*decode_i32)(std::uintptr_t encoded_field) = nullptr;
    void (*encode_i32)(std::uintptr_t encoded_field,
                       std::int32_t value) = nullptr;

    std::int32_t (*get_screen_x)() = nullptr;
    std::int32_t (*get_skill_cooldown)(std::uintptr_t skill) = nullptr;
    void (*draw_skill_icon)(const DrawSkillArgs& args) = nullptr;
    void (*draw_item_slot)(std::int32_t slot_id, std::int32_t x,
                           std::int32_t y, std::int32_t flags) = nullptr;
    void (*draw_text)(std::int32_t x, std::int32_t y,
                      std::int32_t color, const wchar_t* text) = nullptr;
    void (*draw_tips)(std::int32_t x, std::int32_t y,
                      std::int32_t width, std::int32_t style,
                      const wchar_t* text, std::int32_t arg6,
                      std::int32_t arg7) = nullptr;
    bool (*draw_image)(const DrawImageArgs& args) = nullptr;
    void (*send_notice)(const wchar_t* text, std::int32_t color,
                        std::int32_t channel) = nullptr;

    void (*packet_begin)(std::int32_t message_id) = nullptr;
    void (*packet_write)(std::int32_t value, std::int32_t width_type) = nullptr;
    void (*packet_send)() = nullptr;

    std::int32_t (*lookup_info_content)(const wchar_t* key) = nullptr;
    std::int32_t (*get_info_from_data)(std::int32_t index) = nullptr;
    const wchar_t* (*get_character_name)() = nullptr;
    std::int32_t (*get_game_state)() = nullptr;
    std::int32_t (*get_town_index)() = nullptr;
    std::int32_t (*get_area_index)() = nullptr;
    std::int32_t (*get_item_slot_index)(std::uintptr_t object,
                                        std::int32_t slot_type) = nullptr;

    bool (*save_skill_data)(std::int32_t a, std::int32_t b,
                            std::int32_t c, std::int32_t d) = nullptr;
};

struct ExtensionContext {
    VmApi vm;
    HostApi host;
};

void SetExtensionContext(ExtensionContext* context);
bool RegisterSquirrelExtensions(VmHandle vm);

} // namespace dofext

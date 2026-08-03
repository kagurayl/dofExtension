#include "host/absolute_host_functions.hpp"
#include "host/host_addresses.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cwchar>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace dofext::host_detail {
namespace {

std::mutex g_skill_data_mutex;
std::map<std::string, std::vector<std::int32_t>> g_skill_data;
bool g_skill_data_loaded = false;

std::string MakeCharacterDataKey(const wchar_t* name) {
    if (!name) return {};
    const int wide_length = static_cast<int>(std::wcslen(name));
    if (wide_length <= 0) return {};
    const int byte_length = WideCharToMultiByte(
        CP_UTF8, 0, name, wide_length, nullptr, 0, nullptr, nullptr);
    if (byte_length <= 0) return {};
    std::string utf8(static_cast<std::size_t>(byte_length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, name, wide_length,
                        &utf8[0], byte_length, nullptr, nullptr);
    std::string key;
    key.reserve(utf8.size() * 6);
    for (const unsigned char byte : utf8) {
        key += "\\x";
        key += std::to_string(byte >> 4);
        key += std::to_string(byte & 0x0F);
    }
    return key;
}

void LoadSkillDataLocked() {
    if (g_skill_data_loaded) return;
    g_skill_data_loaded = true;
    std::ifstream input("SoundPacks/sounds_effect_chr.npk", std::ios::binary);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto colon = line.find(':');
        const auto left = line.find('[', colon == std::string::npos ? 0 : colon);
        const auto right = line.find(']', left == std::string::npos ? 0 : left);
        if (colon == std::string::npos || left == std::string::npos ||
            right == std::string::npos || right <= left) continue;
        std::string body = line.substr(left + 1, right - left - 1);
        for (char& ch : body) if (ch == ',') ch = ' ';
        std::istringstream parser(body);
        std::vector<std::int32_t> values;
        std::int32_t value = 0;
        while (parser >> value) values.push_back(value);
        if (!values.empty()) g_skill_data[line.substr(0, colon)] = std::move(values);
    }
}

void SaveSkillDataLocked() {
    std::ofstream output("SoundPacks/sounds_effect_chr.npk",
                         std::ios::binary | std::ios::trunc);
    if (!output) return;
    for (const auto& entry : g_skill_data) {
        output << entry.first << ": [";
        for (std::size_t i = 0; i < entry.second.size(); ++i) {
            if (i) output << ", ";
            output << entry.second[i];
        }
        output << "]\n";
    }
}

} // namespace

std::int32_t AbsoluteLookupInfo(const wchar_t* key) {
    if (!key) return 0;
    using QueryFn = void(__thiscall*)(void*, const wchar_t*, char**, std::int32_t*);
    using FreeFn = void(__cdecl*)(void*);
    char* buffer = nullptr;
    std::int32_t length = 0;
    reinterpret_cast<QueryFn>(address::kInfoContentQuery)(
        reinterpret_cast<void*>(address::kInfoContentContext), key, &buffer, &length);
    const auto result = buffer && length > 0 ? length : 0;
    if (buffer) reinterpret_cast<FreeFn>(address::kGameBufferFree)(buffer);
    return result;
}

std::int32_t AbsoluteGetInfoFromData(std::int32_t index) {
    std::lock_guard<std::mutex> lock(g_skill_data_mutex);
    LoadSkillDataLocked();
    if (index < 0 || index > 3) return -1;
    const auto key = MakeCharacterDataKey(AbsoluteCharacterName());
    const auto found = g_skill_data.find(key);
    if (found == g_skill_data.end() ||
        static_cast<std::size_t>(index) >= found->second.size()) return -1;
    return found->second[static_cast<std::size_t>(index)];
}

bool AbsoluteSaveSkillData(std::int32_t a, std::int32_t b,
                           std::int32_t c, std::int32_t d) {
    std::lock_guard<std::mutex> lock(g_skill_data_mutex);
    LoadSkillDataLocked();
    const auto key = MakeCharacterDataKey(AbsoluteCharacterName());
    g_skill_data[key] = {a, b, c, d};
    SaveSkillDataLocked();
    return true;
}

} // namespace dofext::host_detail

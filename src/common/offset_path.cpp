#include "common/offset_path.hpp"

#include <cwchar>
#include <string>

namespace dofext {

std::vector<std::int32_t> ParseOffsetPath(const wchar_t* path) {
    std::vector<std::int32_t> offsets;
    if (!path || !*path) return offsets;

    const wchar_t* token_begin = path;
    for (;;) {
        const wchar_t* token_end = std::wcschr(token_begin, L'+');
        std::wstring token(token_begin,
            token_end ? token_end : token_begin + std::wcslen(token_begin));
        const auto first_non_space = token.find_first_not_of(L' ');
        token = token.substr(first_non_space);
        offsets.push_back(static_cast<std::int32_t>(std::stol(token, nullptr, 0)));

        if (!token_end || token_end[1] == L'\0') break;
        token_begin = token_end + 1;
    }
    return offsets;
}

} // namespace dofext

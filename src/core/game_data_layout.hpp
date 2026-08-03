#pragma once

#include <cstdint>

namespace dofext::layout {

inline constexpr std::uint32_t kStaticTableBias = 4357;
inline constexpr std::uint32_t kStaticDataOffset = 956;
inline constexpr std::uint32_t kLevelDataOffset = 976;
inline constexpr std::uint32_t kMirrorIndexFieldOffset = 1304;

} // namespace dofext::layout

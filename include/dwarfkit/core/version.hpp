#pragma once

#include <string_view>

namespace dwarfkit {

inline constexpr std::string_view versionString = "1.0.0";

std::string_view version();

}  // namespace dwarfkit

#pragma once

#include <string>
#include <string_view>

#include "core/rng.hpp"

namespace as::domain {

// "Guest-" + 4 random characters from an alphabet without look-alikes (no 0/O, 1/I): e.g. "Guest-K7QX".
inline constexpr std::string_view GUEST_PREFIX = "Guest-";
inline constexpr std::string_view GUEST_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
inline constexpr std::size_t GUEST_SUFFIX_LEN = 4;

std::string randomGuestName(core::Rng& rng);
bool isGuestName(std::string_view s);

}  // namespace as::domain

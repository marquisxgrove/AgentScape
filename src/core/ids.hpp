#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "core/rng.hpp"

namespace as::core {

// 26-character ULID (Crockford base32): 48-bit millisecond timestamp + 80 random bits.
// IDs generated later sort lexicographically after earlier ones (to the second here, since the
// injected Clock has 1 s resolution). Seed rows use "seed:..." ids, which can never be ULIDs.
std::string newUlid(std::chrono::sys_seconds now, Rng& rng);

bool isUlid(std::string_view s);

}  // namespace as::core

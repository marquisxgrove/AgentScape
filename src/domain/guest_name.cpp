#include "domain/guest_name.hpp"

namespace as::domain {

std::string randomGuestName(core::Rng& rng) {
  std::string name{GUEST_PREFIX};
  for (std::size_t i = 0; i < GUEST_SUFFIX_LEN; ++i) {
    name += GUEST_ALPHABET[static_cast<std::size_t>(rng.below(GUEST_ALPHABET.size()))];
  }
  return name;
}

bool isGuestName(std::string_view s) {
  if (s.size() != GUEST_PREFIX.size() + GUEST_SUFFIX_LEN || s.substr(0, GUEST_PREFIX.size()) != GUEST_PREFIX) {
    return false;
  }
  for (const char c : s.substr(GUEST_PREFIX.size())) {
    if (GUEST_ALPHABET.find(c) == std::string_view::npos) return false;
  }
  return true;
}

}  // namespace as::domain

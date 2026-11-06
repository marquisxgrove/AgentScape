#include "core/rng.hpp"

#include <random>

namespace as::core {

std::uint64_t Rng::below(std::uint64_t bound) {
  // Rejection sampling: discard the biased tail of the 64-bit range.
  const std::uint64_t limit = UINT64_MAX - (UINT64_MAX % bound);
  std::uint64_t v = nextU64();
  while (v >= limit) v = nextU64();
  return v % bound;
}

std::uint64_t SplitMix64::nextU64() {
  state_ += 0x9E3779B97F4A7C15ULL;
  std::uint64_t z = state_;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

namespace {
std::uint64_t entropySeed() {
  std::random_device rd;
  return (static_cast<std::uint64_t>(rd()) << 32) ^ rd();
}
}  // namespace

SystemRng::SystemRng() : inner_(entropySeed()) {}

}  // namespace as::core

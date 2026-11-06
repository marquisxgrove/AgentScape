#pragma once

#include <chrono>
#include <cstdint>

namespace as::core {

// Injected everywhere randomness or "now" is needed, so domain/services stay deterministic in tests.
class Rng {
 public:
  virtual ~Rng() = default;
  virtual std::uint64_t nextU64() = 0;

  // Uniform in [0, bound) without modulo bias. bound must be > 0.
  std::uint64_t below(std::uint64_t bound);
};

// SplitMix64: tiny, fast, and fully determined by its seed.
class SplitMix64 final : public Rng {
 public:
  explicit SplitMix64(std::uint64_t seed) : state_(seed) {}
  std::uint64_t nextU64() override;

 private:
  std::uint64_t state_;
};

// Seeded from std::random_device once; use only at the app edge.
class SystemRng final : public Rng {
 public:
  SystemRng();
  std::uint64_t nextU64() override { return inner_.nextU64(); }

 private:
  SplitMix64 inner_;
};

class Clock {
 public:
  virtual ~Clock() = default;
  virtual std::chrono::sys_seconds now() const = 0;
};

class SystemClock final : public Clock {
 public:
  std::chrono::sys_seconds now() const override {
    return std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
  }
};

// Settable clock for tests.
class FixedClock final : public Clock {
 public:
  explicit FixedClock(std::chrono::sys_seconds t) : t_(t) {}
  std::chrono::sys_seconds now() const override { return t_; }
  void set(std::chrono::sys_seconds t) { t_ = t; }
  void advance(std::chrono::seconds d) { t_ += d; }

 private:
  std::chrono::sys_seconds t_;
};

}  // namespace as::core

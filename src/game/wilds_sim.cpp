#include "game/wilds_sim.hpp"

#include <algorithm>
#include <cmath>

namespace as::game {

namespace {
constexpr std::array<BugMonster, 5> BESTIARY{{
    {"Null-Pointer Imp", 16, "It points at nothing, and hits like it."},
    {"Off-by-One Goblin", 20, "Always one step ahead. Or behind."},
    {"Memory-Leak Slime", 28, "It never lets go of anything."},
    {"Race-Condition Wraith", 24, "Sometimes there are two of it. Sometimes none."},
    {"Heisenbug", 32, "Vanishes the moment you look at it closely."},
}};

// SplitMix64 step: a good, cheap mix for per-fight randomness.
std::uint64_t mix(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

int monsterFor(std::uint64_t seed, std::int64_t fight) {
  return static_cast<int>(mix(seed ^ static_cast<std::uint64_t>(fight) * 0x100000001B3ULL) % BESTIARY.size());
}

SkillSpot spotFor(std::uint64_t seed, std::int64_t rotation) {
  return static_cast<SkillSpot>((static_cast<std::uint64_t>(rotation) + seed % 3) % 3);
}
}  // namespace

std::span<const BugMonster> bestiary() { return BESTIARY; }

std::uint64_t wildsSeed(std::string_view sessionId) {
  std::uint64_t h = 0xCBF29CE484222325ULL;
  for (const char c : sessionId) {
    h ^= static_cast<unsigned char>(c);
    h *= 0x100000001B3ULL;
  }
  return h;
}

std::array<int, HITS_PER_FIGHT> fightDamage(std::uint64_t seed, std::int64_t fight) {
  const int hp = BESTIARY[static_cast<std::size_t>(monsterFor(seed, fight))].hp;
  std::array<int, HITS_PER_FIGHT> d{};
  for (int k = 0; k < HITS_PER_FIGHT; ++k) d[static_cast<std::size_t>(k)] = hp * (k + 1) / HITS_PER_FIGHT - hp * k / HITS_PER_FIGHT;
  // Shuffle a little damage between neighbouring hits so they don't all look alike; the sum stays exactly hp.
  std::uint64_t bits = mix(seed + static_cast<std::uint64_t>(fight));
  for (std::size_t k = 0; k + 1 < d.size(); k += 2) {
    const int shift = static_cast<int>(bits % 3);   // 0..2
    bits >>= 2;
    const std::size_t from = (bits & 1U) != 0 ? k : k + 1;
    const std::size_t to = from == k ? k + 1 : k;
    bits >>= 1;
    const int moved = std::min(shift, d[from] - 1);
    d[from] -= moved;
    d[to] += moved;
  }
  return d;
}

CombatFrame combatAt(std::uint64_t seed, double elapsed, bool withFamiliar) {
  CombatFrame f;
  const double t = std::max(0.0, elapsed);
  f.fight = static_cast<std::int64_t>(std::floor(t / FIGHT_SECONDS));
  const double inFight = t - static_cast<double>(f.fight) * FIGHT_SECONDS;
  f.monster = monsterFor(seed, f.fight);
  f.hpMax = BESTIARY[static_cast<std::size_t>(f.monster)].hp;
  const auto damage = fightDamage(seed, f.fight);
  const int landed = std::min(HITS_PER_FIGHT, static_cast<int>(std::floor(inFight / HIT_SECONDS + 1e-9)));
  int dealt = 0;
  for (int k = 0; k < landed; ++k) dealt += damage[static_cast<std::size_t>(k)];
  f.hpLeft = f.hpMax - dealt;
  f.squashed = landed == HITS_PER_FIGHT;
  if (landed > 0) {
    Hit h;
    h.damage = damage[static_cast<std::size_t>(landed - 1)];
    h.byFamiliar = withFamiliar && (landed % 2 == 0);
    h.age = inFight - landed * HIT_SECONDS;
    f.lastHit = h;
  }
  f.squashedTotal = f.fight + (f.squashed ? 1 : 0);
  return f;
}

std::string_view skillName(SkillSpot spot) {
  switch (spot) {
    case SkillSpot::Tree: return "Woodcutting";
    case SkillSpot::Rock: return "Mining";
    case SkillSpot::Pond: return "Fishing";
  }
  return "Woodcutting";
}

std::string_view skillResource(SkillSpot spot) {
  switch (spot) {
    case SkillSpot::Tree: return "Logs";
    case SkillSpot::Rock: return "Ore";
    case SkillSpot::Pond: return "Fish";
  }
  return "Logs";
}

SkillFrame skillingAt(std::uint64_t seed, double elapsed) {
  SkillFrame f;
  const double t = std::max(0.0, elapsed);
  const auto rotation = static_cast<std::int64_t>(std::floor(t / SKILL_ROTATE_SECONDS));
  f.spot = spotFor(seed, rotation);
  const auto perRotation = static_cast<std::int64_t>(SKILL_ROTATE_SECONDS / SKILL_ACTION_SECONDS);
  const auto add = [&](SkillSpot spot, std::int64_t n) {
    switch (spot) {
      case SkillSpot::Tree: f.tally.logs += n; break;
      case SkillSpot::Rock: f.tally.ore += n; break;
      case SkillSpot::Pond: f.tally.fish += n; break;
    }
  };
  // Whole rotations come in one go per spot (three spots repeat), so this is O(1) however long the session ran.
  for (std::int64_t r = 0; r < std::min<std::int64_t>(rotation, 3); ++r) {
    const std::int64_t times = (rotation - r + 2) / 3;   // rotations r, r+3, r+6, ... before `rotation`
    add(spotFor(seed, r), times * perRotation);
  }
  const double inRotation = t - static_cast<double>(rotation) * SKILL_ROTATE_SECONDS;
  const auto done = static_cast<std::int64_t>(std::floor(inRotation / SKILL_ACTION_SECONDS + 1e-9));
  add(f.spot, done);
  f.progress = (inRotation - static_cast<double>(done) * SKILL_ACTION_SECONDS) / SKILL_ACTION_SECONDS;
  if (done > 0) f.sinceGather = inRotation - static_cast<double>(done) * SKILL_ACTION_SECONDS;
  return f;
}

}  // namespace as::game

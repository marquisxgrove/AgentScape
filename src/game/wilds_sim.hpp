#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace as::game {

// What the Adventurer does in The Wilds while a coding session runs (coding-sessions Requirement 3). Pure and
// deterministic: everything follows from the session's seed and its elapsed time, so a restart shows the same fight
// and the same tallies (Requirement 3.2). Nothing here touches gameplay; it is what the scene draws.

struct BugMonster {
  std::string_view name;
  int hp;
  std::string_view examine;
};

// The bug monsters, in the order they appear. Their sprites are the rows of assets/sprites/bugs.png.
std::span<const BugMonster> bestiary();

inline constexpr double FIGHT_SECONDS = 12.0;     // one monster every 12 s
inline constexpr int HITS_PER_FIGHT = 8;
inline constexpr double HIT_SECONDS = 1.4;        // hits land at 1.4, 2.8, ... 11.2 s; the last one squashes it
inline constexpr double SKILL_ACTION_SECONDS = 5.0;
inline constexpr double SKILL_ROTATE_SECONDS = 600.0;   // a different skill every 10 minutes

// A stable seed from a session id (FNV-1a).
std::uint64_t wildsSeed(std::string_view sessionId);

struct Hit {
  int damage = 0;
  bool byFamiliar = false;
  double age = 0;   // seconds since it landed
};

struct CombatFrame {
  std::int64_t fight = 0;        // index of the current fight
  int monster = 0;               // index into bestiary()
  int hpMax = 1;
  int hpLeft = 1;
  bool squashed = false;         // between the last hit and the next monster
  std::optional<Hit> lastHit;    // the most recent hit of this fight
  std::int64_t squashedTotal = 0;   // monsters squashed in [0, elapsed]
};

// The fight at `elapsed` seconds into a session. With a familiar, it lands every other hit.
CombatFrame combatAt(std::uint64_t seed, double elapsed, bool withFamiliar);

// The damage of each hit of one fight: positive, and adding up to the monster's hp exactly.
std::array<int, HITS_PER_FIGHT> fightDamage(std::uint64_t seed, std::int64_t fight);

enum class SkillSpot { Tree, Rock, Pond };
std::string_view skillName(SkillSpot spot);        // "Woodcutting", "Mining", "Fishing"
std::string_view skillResource(SkillSpot spot);    // "Logs", "Ore", "Fish"

struct SkillTally {
  std::int64_t logs = 0;
  std::int64_t ore = 0;
  std::int64_t fish = 0;
};

struct SkillFrame {
  SkillSpot spot = SkillSpot::Tree;
  double progress = 0;             // through the current action, 0..1
  double sinceGather = 1e9;        // seconds since the last resource landed (large: none yet this rotation)
  SkillTally tally;                // gathered in [0, elapsed]
};

// The skilling at `elapsed` seconds into a solo session.
SkillFrame skillingAt(std::uint64_t seed, double elapsed);

}  // namespace as::game

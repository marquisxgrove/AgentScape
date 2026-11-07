#include "game/xp_feedback.hpp"

namespace as::game {

std::string levelUpMessage(int level) {
  return "Congratulations, you just advanced a Questing level! You are now level " + std::to_string(level) + ".";
}

std::string unlockMessage(std::string_view itemName) { return "You have unlocked: " + std::string(itemName) + "!"; }

XpFeedback feedbackFor(std::int64_t previousTotal, std::int64_t newTotal, int oldLevel, int newLevel) {
  XpFeedback f;
  if (newTotal > previousTotal) f.gained = static_cast<int>(newTotal - previousTotal);
  if (newLevel > oldLevel) f.levelUp = newLevel;
  return f;
}

std::string dropText(int gained) { return "+" + std::to_string(gained) + " XP"; }

}  // namespace as::game

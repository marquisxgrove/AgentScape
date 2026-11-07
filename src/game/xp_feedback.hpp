#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace as::game {

// Pure game feedback for XP changes (quest-logs Requirements 5.5, 5.6 and 6.1): what to say and what to show
// when the signed-in user's XP moves. No SDL or ImGui, so the wording and the decisions are unit-tested.

// "Congratulations, you just advanced a Questing level! You are now level N."
std::string levelUpMessage(int level);

// "You have unlocked: <Item>!"
std::string unlockMessage(std::string_view itemName);

struct XpFeedback {
  int gained = 0;                       // > 0: show a floating "+N XP" drop; 0: show none (XP went down or stayed)
  std::optional<int> levelUp;           // the new level, when it rose
};

// Decides the feedback for a change from `previousTotal` to `newTotal` XP. A drop in XP (a deleted Quest Log took
// its XP back) gives no drop and no message. A rise of several levels reports the final level once.
XpFeedback feedbackFor(std::int64_t previousTotal, std::int64_t newTotal, int oldLevel, int newLevel);

// The text of a floating drop: "+250 XP".
std::string dropText(int gained);

}  // namespace as::game

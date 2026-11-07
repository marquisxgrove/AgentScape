#pragma once

#include <imgui.h>

#include <optional>
#include <string>

#include "domain/snapshot.hpp"
#include "services/quest_service.hpp"

namespace as::game {
class AvatarRenderer;
}

namespace as::screens::quest {

// A 0..5 gem rating: "-" and "+" buttons around five gems, then the number ("4/5", or "-" for none).
// The buttons are the keyboard and gamepad controls (two focus stops per rating, not five); the gems themselves
// can be clicked with the mouse. The number is always shown, so the value never depends on colour alone.
// `minValue` is 0 for an optional rating and 1 for a required one. Returns true when the value changed.
bool gemRating(const char* id, int* value, int minValue);

// Five gems showing a 1..5 rating, not interactive (0 draws five empty gems). `size` is the gem width in pixels.
void gemDisplay(ImDrawList* dl, ImVec2 topLeft, int value, float size, ImU32 fill, ImU32 outline);

// An Adventurer_Snapshot drawn through the shared avatar renderer, at `scale` times the sprite size. A snapshot that
// could not be read, or a missing renderer, leaves an empty box of the same size so the layout does not move.
void avatarThumb(game::AvatarRenderer* avatars, const std::optional<domain::AdventurerSnapshot>& snapshot, float scale);

// One pinned parchment note for the Tavern board (Requirement 3.2): the author's thumbnail and name, Agent and
// version, Quest Type, overall gems, the recommend flag, tags, title and an excerpt. Returns true when chosen.
// Draws with ink on parchment, so it reads the same under any palette.
bool questCard(const char* id, const services::QuestLogRecord& log, const std::optional<domain::AdventurerSnapshot>& snapshot,
               game::AvatarRenderer* avatars, bool helpedByViewer, float width);

// The height of one note at the current font size: six lines of text, or the thumbnail and name, whichever is taller.
float questCardHeight();

}  // namespace as::screens::quest

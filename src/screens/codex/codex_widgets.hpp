#pragma once

#include <imgui.h>

#include <optional>
#include <string>

#include "domain/skills.hpp"
#include "ui/palette.hpp"

namespace as::screens::codex {

// A PixelButton sized to its text that stays on the previous line when it fits and wraps when it doesn't.
// `first` is true for the first button of a group (it never joins a previous line).
bool flowButton(const std::string& text, const char* id, bool& first);

// Draws skill icon `index` (0..9, SKILLS order) at the cursor, scaled 1x, and advances the layout.
// Falls back to an empty box if the strip didn't load.
void skillIcon(int index);

// A small filled label such as BOON or BANE, drawn at the cursor. Text colours are chosen for contrast on
// the fill (the palette's red text does not have enough contrast on stone, so Banes use a red fill).
void chip(const char* label, ImU32 fill, ImU32 text);

// "Lv 57" or "-" for an unranked level.
std::string levelText(const std::optional<int>& level);

// Cycles through `count` options after `current`; -1 stands for "none" before index 0.
int cycleNext(int current, int count);

}  // namespace screens::codex

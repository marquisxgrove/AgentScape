#pragma once

namespace as::screens::quest {

// A 0..5 gem rating: "-" and "+" buttons around five gems, then the number ("4/5", or "-" for none).
// The buttons are the keyboard and gamepad controls (two focus stops per rating, not five); the gems themselves
// can be clicked with the mouse. The number is always shown, so the value never depends on colour alone.
// `minValue` is 0 for an optional rating and 1 for a required one. Returns true when the value changed.
bool gemRating(const char* id, int* value, int minValue);

}  // namespace as::screens::quest

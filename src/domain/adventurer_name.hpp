#pragma once

#include <span>
#include <string>
#include <string_view>

#include "core/result.hpp"

namespace as::domain {

enum class NameError { Invalid, Blocked };

// Validates and normalises an Adventurer name (Req 1.1): 1-12 chars, letters/digits/space/_/-,
// must start with a letter or digit, no leading/trailing/consecutive separators. Trims surrounding
// ASCII whitespace first. Rejects names whose nameKey matches the blocklist (compared via nameKey).
// On success returns the trimmed display name.
Result<std::string, NameError> validateName(std::string_view raw, std::span<const std::string_view> blocklist);

// Canonical key for uniqueness (Req 1.2): lowercase; every run of space/_/- collapses to one space.
// Idempotent: nameKey(nameKey(x)) == nameKey(x). Two names conflict iff their keys are equal.
std::string nameKey(std::string_view name);

}  // namespace as::domain

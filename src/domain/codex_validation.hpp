#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "core/result.hpp"

namespace as::domain {

enum class AgentKind { Model, Tool };

struct AgentInput {
  std::string name;
  std::string vendor;
  AgentKind kind = AgentKind::Model;
  std::string examine;
  std::string about;
  std::optional<std::string> website;  // https only
};

// Which field failed and a game-voice message to show next to it.
struct FieldError {
  std::string field;
  std::string message;
};

inline constexpr std::size_t MAX_AGENT_NAME = 40;
inline constexpr std::size_t MAX_AGENT_VENDOR = 40;
inline constexpr std::size_t MAX_EXAMINE = 200;   // Requirement 2.1
inline constexpr std::size_t MAX_ABOUT = 1000;
inline constexpr std::size_t MAX_WEBSITE = 200;
inline constexpr std::size_t MIN_NOTE = 10;       // Requirement 5.1
inline constexpr std::size_t MAX_NOTE = 280;

// Number of Unicode code points, or nullopt if the text is not valid UTF-8 (overlongs, surrogates and
// out-of-range values are rejected).
std::optional<std::size_t> utf8Length(std::string_view s);

// Trims surrounding whitespace and checks every field against its limits. Text must be valid UTF-8 with no
// control characters (examine and name are single-line; about may contain newlines). A website, if given,
// must pass validateWebsite. Returns the cleaned input.
Result<AgentInput, FieldError> validateAgentInput(AgentInput raw);

// https: URLs only (Requirement 4.5): a host with at least one dot, optional port, no spaces, control
// characters or '@' (which could disguise the real host in the confirmation prompt), at most 200 chars.
Result<std::string, FieldError> validateWebsite(std::string_view url);

// The lower-cased host of a URL accepted by validateWebsite, for "Leave AgentScape to visit <host>?".
std::optional<std::string> websiteHost(std::string_view url);

// Boon / Bane text: trimmed, 10..280 code points, valid UTF-8, no control characters (one line).
Result<std::string, FieldError> validateNoteText(std::string_view raw);

}  // namespace as::domain

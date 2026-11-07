#include "domain/hiscores.hpp"

#include <cctype>

namespace as::domain {

namespace {
std::string lower(std::string_view s) {
  std::string out(s);
  for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

std::string trimmed(std::string_view s) {
  std::size_t b = 0;
  std::size_t e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b])) != 0) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])) != 0) --e;
  return std::string(s.substr(b, e - b));
}
}  // namespace

std::optional<int> boardScore(const AgentLevels& levels, const AgentBoard& board) {
  switch (board.kind) {
    case AgentBoardKind::Overall: return levels.overall;
    case AgentBoardKind::Skill: return levels.skills[static_cast<std::size_t>(board.skill)];
    case AgentBoardKind::Total: {
      for (const auto& s : levels.skills) {
        if (s) return levels.total;
      }
      return std::nullopt;
    }
  }
  return std::nullopt;
}

const char* boardName(const AgentBoard& board) {
  switch (board.kind) {
    case AgentBoardKind::Overall: return "Overall";
    case AgentBoardKind::Total: return "Total level";
    case AgentBoardKind::Skill: return displayName(board.skill);
  }
  return "Overall";
}

std::optional<std::size_t> findByName(std::span<const std::string> names, std::string_view rawQuery) {
  const std::string query = lower(trimmed(rawQuery));
  if (query.empty()) return std::nullopt;
  std::optional<std::size_t> prefix;
  std::optional<std::size_t> contains;
  for (std::size_t i = 0; i < names.size(); ++i) {
    const std::string name = lower(names[i]);
    if (name == query) return i;   // the first whole-name match wins outright
    if (!prefix && name.rfind(query, 0) == 0) prefix = i;
    if (!contains && name.find(query) != std::string::npos) contains = i;
  }
  return prefix ? prefix : contains;
}

}  // namespace as::domain

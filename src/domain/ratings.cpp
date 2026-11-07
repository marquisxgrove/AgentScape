#include "domain/ratings.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <variant>

namespace as::domain {

namespace {
using nlohmann::json;

// Position of a stat in display order: Overall first, then the ten Skills.
std::size_t statIndex(const StatKey& key) {
  if (std::holds_alternative<Overall>(key)) return 0;
  return 1 + static_cast<std::size_t>(std::get<Skill>(key));
}

StatKey statAt(std::size_t index) {
  if (index == 0) return Overall{};
  return SKILLS[index - 1];
}

// (Quest Type, stat) -> what one RatingSet adds to that aggregate.
using Key = std::pair<int, std::size_t>;
using Contribution = std::map<Key, std::pair<std::int64_t, std::int64_t>>;   // key -> (sum, count)

void addTo(Contribution& c, const RatingSet& set, int sign) {
  const int qt = static_cast<int>(set.questType);
  auto add = [&](std::size_t stat, Stars stars) {
    auto& cell = c[{qt, stat}];
    cell.first += sign * stars;
    cell.second += sign;
  };
  add(0, set.overall);
  for (const auto& [skill, stars] : set.skills) add(1 + static_cast<std::size_t>(skill), stars);
}

std::vector<Delta> toDeltas(const Contribution& c) {
  std::vector<Delta> out;
  for (const auto& [key, cell] : c) {   // std::map iterates in (Quest Type, stat) order: canonical
    if (cell.first == 0 && cell.second == 0) continue;
    out.push_back({statAt(key.second), static_cast<QuestType>(key.first), cell.first, cell.second});
  }
  return out;
}

bool hasControl(const std::string& s, bool allowNewline) {
  for (const char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    if (c == '\n' && allowNewline) continue;
    if (c < 0x20 || c == 0x7f) return true;
  }
  return false;
}

std::string trimmed(const std::string& s) {
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return {};
  return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

bool validStars(Stars s) { return s >= 1 && s <= 5; }

// Checks a tag list against the fixed list: at most 3, every one on the list, none twice.
template <std::size_t N>
bool validTags(const std::vector<std::string>& tags, const std::array<std::string_view, N>& allowed) {
  if (tags.size() > MAX_TAGS_PER_SIDE) return false;
  for (std::size_t i = 0; i < tags.size(); ++i) {
    if (std::find(allowed.begin(), allowed.end(), tags[i]) == allowed.end()) return false;
    if (std::find(tags.begin(), tags.begin() + static_cast<std::ptrdiff_t>(i), tags[i]) != tags.begin() + static_cast<std::ptrdiff_t>(i)) {
      return false;
    }
  }
  return true;
}
}  // namespace

std::vector<Delta> ratingDeltas(const std::optional<RatingSet>& before, const std::optional<RatingSet>& after) {
  Contribution c;
  if (before && !before->excluded) addTo(c, *before, -1);
  if (after && !after->excluded) addTo(c, *after, +1);
  return toDeltas(c);
}

std::vector<Delta> sumDeltas(std::span<const std::vector<Delta>> lists) {
  Contribution c;
  for (const auto& list : lists) {
    for (const Delta& d : list) {
      auto& cell = c[{static_cast<int>(d.questType), statIndex(d.skill)}];
      cell.first += d.sumDelta;
      cell.second += d.countDelta;
    }
  }
  return toDeltas(c);
}

Result<QuestLogInput, ValidationErrors> validateQuestLogInput(const QuestLogInput& in) {
  ValidationErrors errors;
  const auto bad = [&](const char* field, const char* message) { errors.push_back({field, message}); };
  QuestLogInput out = in;

  if (in.agentId.empty()) bad("agent", "Choose which familiar you went questing with.");

  out.version = trimmed(in.version);
  {
    const auto len = utf8Length(out.version);
    if (!len || hasControl(out.version, false)) {
      bad("version", "That version has characters the scribes can't write.");
    } else if (*len > MAX_VERSION_LEN) {
      bad("version", "That version is too long (40 characters at most).");
    }
  }

  if (!validStars(in.overall)) bad("overall", "Give an overall rating from 1 to 5.");
  for (const auto& [skill, stars] : in.skills) {
    if (!validStars(stars)) {
      bad("skills", "Each skill rating must be from 1 to 5.");
      break;
    }
  }

  out.title = trimmed(in.title);
  {
    const auto len = utf8Length(out.title);
    if (!len || hasControl(out.title, false)) {
      bad("title", "The title has characters the scribes can't write.");
    } else if (*len < MIN_TITLE || *len > MAX_TITLE) {
      bad("title", "The title needs 5 to 80 characters.");
    }
  }

  // Line breaks are kept (as "\n"); every other control character is refused.
  std::string body;
  for (std::size_t i = 0; i < in.body.size(); ++i) {
    if (in.body[i] == '\r' && i + 1 < in.body.size() && in.body[i + 1] == '\n') continue;   // "\r\n" -> "\n"
    body += in.body[i];
  }
  out.body = trimmed(body);
  {
    const auto len = utf8Length(out.body);
    if (!len || hasControl(out.body, true)) {
      bad("body", "The story has characters the scribes can't write.");
    } else if (*len < MIN_BODY || *len > MAX_BODY) {
      bad("body", "The story needs 20 to 2,000 characters.");
    }
  }

  if (!validTags(in.pros, PRO_TAGS)) bad("pros", "Pick up to 3 different good points from the list.");
  if (!validTags(in.cons, CON_TAGS)) bad("cons", "Pick up to 3 different bad points from the list.");

  if (!errors.empty()) return tl::unexpected(std::move(errors));
  return out;
}

RatingSet ratingSetOf(const QuestLogInput& input) {
  return RatingSet{input.questType, input.overall, input.skills, false};
}

std::string ratingsToJson(const std::map<Skill, Stars>& skills) {
  json j = json::object();
  for (const auto& [skill, stars] : skills) j[storageName(StatKey{skill})] = stars;
  return j.dump();
}

std::map<Skill, Stars> ratingsFromJson(std::string_view text) {
  std::map<Skill, Stars> out;
  const auto j = json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object()) return out;
  for (const auto& [name, value] : j.items()) {
    const auto key = statKeyFromStorage(name);
    if (!key || !std::holds_alternative<Skill>(*key) || !value.is_number_integer()) continue;
    const int stars = value.get<int>();
    if (validStars(stars)) out[std::get<Skill>(*key)] = stars;
  }
  return out;
}

std::string tagsToJson(const std::vector<std::string>& pros, const std::vector<std::string>& cons) {
  json j = json::array();
  for (const auto& t : pros) j.push_back(t);
  for (const auto& t : cons) j.push_back(t);
  return j.dump();
}

void tagsFromJson(std::string_view text, std::vector<std::string>& pros, std::vector<std::string>& cons) {
  pros.clear();
  cons.clear();
  const auto j = json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (!j.is_array()) return;
  for (const auto& v : j) {
    if (!v.is_string()) continue;
    const auto s = v.get<std::string>();
    if (std::find(PRO_TAGS.begin(), PRO_TAGS.end(), s) != PRO_TAGS.end()) pros.push_back(s);
    if (std::find(CON_TAGS.begin(), CON_TAGS.end(), s) != CON_TAGS.end()) cons.push_back(s);
  }
}

std::string questLogInputToJson(const QuestLogInput& in) {
  json j;
  j["agentId"] = in.agentId;
  j["version"] = in.version;
  j["questType"] = storageName(in.questType);
  j["overall"] = in.overall;
  j["skills"] = json::parse(ratingsToJson(in.skills));
  j["title"] = in.title;
  j["body"] = in.body;
  j["recommend"] = in.recommend;
  j["pros"] = in.pros;
  j["cons"] = in.cons;
  return j.dump();
}

std::optional<QuestLogInput> questLogInputFromJson(std::string_view text) {
  const auto j = json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object()) return std::nullopt;
  try {
    QuestLogInput in;
    in.agentId = j.at("agentId").get<std::string>();
    in.version = j.at("version").get<std::string>();
    const auto qt = questTypeFromStorage(j.at("questType").get<std::string>());
    if (!qt) return std::nullopt;
    in.questType = *qt;
    in.overall = j.at("overall").get<int>();
    in.skills = ratingsFromJson(j.at("skills").dump());
    in.title = j.at("title").get<std::string>();
    in.body = j.at("body").get<std::string>();
    in.recommend = j.at("recommend").get<bool>();
    in.pros = j.at("pros").get<std::vector<std::string>>();
    in.cons = j.at("cons").get<std::vector<std::string>>();
    return in;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

}  // namespace as::domain

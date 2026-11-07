#include "db/seed_loader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <optional>

#include "core/civil_time.hpp"
#include "domain/codex_validation.hpp"
#include "domain/skills.hpp"
#include "domain/slug.hpp"

namespace as::db {

namespace {

using nlohmann::json;

struct NpcSeed {
  std::string id;
  std::string name;
};

struct AgentSeed {
  std::string id;  // seed:agent:<slug>
  domain::AgentInput input;
  std::string slug;
};

struct StatSeed {
  std::string agentSlug;
  std::string stat;       // storage names, validated
  std::string questType;
  std::int64_t sum = 0;
  std::int64_t count = 0;
};

struct NoteSeed {
  std::string id;
  std::string agentSlug;
  std::string authorId;
  std::string kind;  // BOON / BANE
  std::optional<std::string> skill;
  std::string text;
  std::int64_t agree = 0;
  std::int64_t disagree = 0;
};

struct ParsedSeed {
  std::string snapshotAt;  // ISO-8601, used as created_at/last_seen_at for NPC rows
  std::vector<NpcSeed> npcs;
  std::string agentsAt;
  std::vector<AgentSeed> agents;
  std::vector<StatSeed> stats;
  std::string notesAt;
  std::vector<NoteSeed> notes;
};

// Files are applied in this order (later files reference earlier ones: quest logs need NPCs and
// Agents). New seed files are added here as their specs land (agent-codex: agents.json, notes.json;
// quest-logs: quest_logs.json).
constexpr std::array<std::string_view, 4> KNOWN_FILES{"npcs.json", "agents.json", "ratings.json", "notes.json"};

Result<void> invalid(std::string_view file, std::string_view detail) {
  return makeError(GameErrorCode::InvalidInput, "Your save could not be loaded.",
                   "seed file " + std::string(file) + ": " + std::string(detail));
}

bool isSlugChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; }

bool validSeedId(std::string_view id, std::string_view kind) {
  const std::string prefix = std::string(SEED_ID_PREFIX) + std::string(kind) + ":";
  if (id.size() <= prefix.size() || id.substr(0, prefix.size()) != prefix) return false;
  const auto slug = id.substr(prefix.size());
  return std::all_of(slug.begin(), slug.end(), isSlugChar);
}

Result<void> parseNpcs(std::string_view file, const json& doc, ParsedSeed& out) {
  if (!doc.is_object()) return invalid(file, "expected an object");
  const auto snap = doc.find("snapshotAt");
  if (snap == doc.end() || !snap->is_string() || !core::parseIso8601(snap->get<std::string>())) {
    return invalid(file, "snapshotAt must be an ISO-8601 UTC timestamp");
  }
  out.snapshotAt = snap->get<std::string>();
  const auto list = doc.find("npcs");
  if (list == doc.end() || !list->is_array()) return invalid(file, "npcs must be an array");

  std::vector<std::string> seen;
  for (const auto& item : *list) {
    if (!item.is_object() || !item.contains("id") || !item.contains("name") || !item["id"].is_string() ||
        !item["name"].is_string()) {
      return invalid(file, "each npc needs string id and name");
    }
    NpcSeed npc{item["id"].get<std::string>(), item["name"].get<std::string>()};
    if (!validSeedId(npc.id, "npc")) return invalid(file, "bad npc id '" + npc.id + "' (want seed:npc:<slug>)");
    // Placeholder bounds until character-creator's validateName lands (it applies to NPCs too).
    if (npc.name.empty() || npc.name.size() > 24) return invalid(file, "npc name must be 1-24 bytes: " + npc.id);
    if (std::find(seen.begin(), seen.end(), npc.id) != seen.end()) return invalid(file, "duplicate id " + npc.id);
    seen.push_back(npc.id);
    out.npcs.push_back(std::move(npc));
  }
  return {};
}

Result<std::string> snapshotOf(std::string_view file, const json& doc) {
  if (!doc.is_object()) return tl::unexpected(invalid(file, "expected an object").error());
  const auto snap = doc.find("snapshotAt");
  if (snap == doc.end() || !snap->is_string() || !core::parseIso8601(snap->get<std::string>())) {
    return tl::unexpected(invalid(file, "snapshotAt must be an ISO-8601 UTC timestamp").error());
  }
  return snap->get<std::string>();
}

std::optional<std::string> stringField(const json& obj, const char* key) {
  const auto it = obj.find(key);
  if (it == obj.end() || !it->is_string()) return std::nullopt;
  return it->get<std::string>();
}

std::optional<std::int64_t> intField(const json& obj, const char* key) {
  const auto it = obj.find(key);
  if (it == obj.end() || !it->is_number_integer()) return std::nullopt;
  return it->get<std::int64_t>();
}

Result<void> parseAgents(std::string_view file, const json& doc, ParsedSeed& out) {
  auto snap = snapshotOf(file, doc);
  if (!snap) return tl::unexpected(snap.error());
  out.agentsAt = *snap;
  const auto list = doc.find("agents");
  if (list == doc.end() || !list->is_array()) return invalid(file, "agents must be an array");

  std::vector<std::string> seen;
  for (const auto& item : *list) {
    if (!item.is_object()) return invalid(file, "each agent must be an object");
    const auto slug = stringField(item, "slug");
    const auto name = stringField(item, "name");
    const auto vendor = stringField(item, "vendor");
    const auto kind = stringField(item, "kind");
    const auto examine = stringField(item, "examine");
    if (!slug || !name || !vendor || !kind || !examine || (*kind != "MODEL" && *kind != "TOOL")) {
      return invalid(file, "each agent needs slug, name, vendor, kind (MODEL|TOOL) and examine");
    }
    // Seed Agents obey exactly the rules a player's proposal does.
    domain::AgentInput input;
    input.name = *name;
    input.vendor = *vendor;
    input.kind = *kind == "TOOL" ? domain::AgentKind::Tool : domain::AgentKind::Model;
    input.examine = *examine;
    input.about = stringField(item, "about").value_or("");
    input.website = stringField(item, "website");
    auto valid = domain::validateAgentInput(input);
    if (!valid) return invalid(file, "agent '" + *slug + "' field " + valid.error().field + ": " + valid.error().message);
    if (domain::slugify(valid->name) != *slug) return invalid(file, "slug '" + *slug + "' must equal slugify(name)");
    if (std::find(seen.begin(), seen.end(), *slug) != seen.end()) return invalid(file, "duplicate agent " + *slug);
    seen.push_back(*slug);
    out.agents.push_back({std::string(SEED_ID_PREFIX) + "agent:" + *slug, std::move(*valid), *slug});
  }
  return {};
}

Result<void> parseRatings(std::string_view file, const json& doc, ParsedSeed& out) {
  auto snap = snapshotOf(file, doc);
  if (!snap) return tl::unexpected(snap.error());
  const auto agents = doc.find("agents");
  if (agents == doc.end() || !agents->is_object()) return invalid(file, "agents must be an object");
  for (const auto& [slug, stats] : agents->items()) {
    if (!stats.is_object()) return invalid(file, "ratings for '" + slug + "' must be an object");
    for (const auto& [statName, byType] : stats.items()) {
      if (!domain::statKeyFromStorage(statName)) return invalid(file, "unknown stat '" + statName + "'");
      if (!byType.is_object()) return invalid(file, "stat '" + statName + "' must map quest types to [sum, count]");
      for (const auto& [typeName, pair] : byType.items()) {
        if (!domain::questTypeFromStorage(typeName)) return invalid(file, "unknown quest type '" + typeName + "'");
        if (!pair.is_array() || pair.size() != 2 || !pair[0].is_number_integer() || !pair[1].is_number_integer()) {
          return invalid(file, "ratings must be [sum, count] integers");
        }
        const auto sum = pair[0].get<std::int64_t>();
        const auto count = pair[1].get<std::int64_t>();
        // A possible set of `count` ratings in 1..5 (the table's CHECK constraint, caught here with a clear message).
        if (count < 0 || sum < count || sum > 5 * count) return invalid(file, "impossible ratings for '" + slug + "' " + statName);
        out.stats.push_back({slug, statName, typeName, sum, count});
      }
    }
  }
  return {};
}

Result<void> parseNotes(std::string_view file, const json& doc, ParsedSeed& out) {
  auto snap = snapshotOf(file, doc);
  if (!snap) return tl::unexpected(snap.error());
  out.notesAt = *snap;
  const auto list = doc.find("notes");
  if (list == doc.end() || !list->is_array()) return invalid(file, "notes must be an array");

  std::vector<std::string> seen;
  for (const auto& item : *list) {
    if (!item.is_object()) return invalid(file, "each note must be an object");
    const auto id = stringField(item, "id");
    const auto agent = stringField(item, "agent");
    const auto author = stringField(item, "author");
    const auto kind = stringField(item, "kind");
    const auto text = stringField(item, "text");
    const auto agree = intField(item, "agree");
    const auto disagree = intField(item, "disagree");
    if (!id || !agent || !author || !kind || !text || !agree || !disagree || (*kind != "BOON" && *kind != "BANE")) {
      return invalid(file, "each note needs id, agent, author, kind (BOON|BANE), text, agree and disagree");
    }
    if (!validSeedId(*id, "note")) return invalid(file, "bad note id '" + *id + "' (want seed:note:<slug>)");
    if (*agree < 0 || *disagree < 0) return invalid(file, "vote counts cannot be negative: " + *id);
    auto cleaned = domain::validateNoteText(*text);
    if (!cleaned) return invalid(file, "note " + *id + ": " + cleaned.error().message);
    std::optional<std::string> skill = stringField(item, "skill");
    if (skill) {
      const auto key = domain::statKeyFromStorage(*skill);
      if (!key || std::holds_alternative<domain::Overall>(*key)) return invalid(file, "note " + *id + " has a bad skill");
    }
    if (std::find(seen.begin(), seen.end(), *id) != seen.end()) return invalid(file, "duplicate note " + *id);
    seen.push_back(*id);
    out.notes.push_back({*id, *agent, *author, *kind, std::move(skill), *cleaned, *agree, *disagree});
  }
  return {};
}

// References between files must resolve, so a typo can't silently drop data.
Result<void> checkReferences(const ParsedSeed& p) {
  const auto hasAgent = [&](const std::string& slug) {
    return std::any_of(p.agents.begin(), p.agents.end(), [&](const AgentSeed& a) { return a.slug == slug; });
  };
  for (const auto& s : p.stats) {
    if (!hasAgent(s.agentSlug)) return invalid("ratings.json", "unknown agent '" + s.agentSlug + "'");
  }
  for (const auto& n : p.notes) {
    if (!hasAgent(n.agentSlug)) return invalid("notes.json", "note " + n.id + " has unknown agent '" + n.agentSlug + "'");
    if (std::none_of(p.npcs.begin(), p.npcs.end(), [&](const NpcSeed& npc) { return npc.id == n.authorId; })) {
      return invalid("notes.json", "note " + n.id + " has unknown author '" + n.authorId + "'");
    }
  }
  return {};
}

Result<ParsedSeed> parseAll(std::span<const SeedFile> files, std::vector<std::string>& order) {
  ParsedSeed parsed;
  for (const auto& f : files) {
    if (std::find(KNOWN_FILES.begin(), KNOWN_FILES.end(), f.name) == KNOWN_FILES.end()) {
      return makeError(GameErrorCode::InvalidInput, "Your save could not be loaded.",
                       "seed file " + std::string(f.name) + " has no loader");
    }
  }
  for (const auto known : KNOWN_FILES) {
    const auto it = std::find_if(files.begin(), files.end(), [&](const SeedFile& f) { return f.name == known; });
    if (it == files.end()) continue;
    json doc = json::parse(it->json, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded()) {
      return makeError(GameErrorCode::InvalidInput, "Your save could not be loaded.",
                       "seed file " + std::string(known) + ": invalid JSON");
    }
    Result<void> r;
    if (known == "npcs.json") r = parseNpcs(known, doc, parsed);
    if (known == "agents.json") r = parseAgents(known, doc, parsed);
    if (known == "ratings.json") r = parseRatings(known, doc, parsed);
    if (known == "notes.json") r = parseNotes(known, doc, parsed);
    if (!r) return tl::unexpected(r.error());
    order.emplace_back(known);
  }
  if (auto refs = checkReferences(parsed); !refs) return tl::unexpected(refs.error());
  return parsed;
}

void setAppState(Database& db, std::string_view key, std::string_view value) {
  db.prepare("INSERT INTO app_state(key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value")
      .bind(1, key)
      .bind(2, value)
      .run();
}

}  // namespace

bool needsSeed(Database& db) {
  auto st = db.prepare("SELECT value FROM app_state WHERE key = 'seed_version'");
  if (!st.step()) return true;
  return st.columnText(0) != std::to_string(SEED_VERSION);
}

Result<SeedReport> runSeed(Database& db, std::span<const SeedFile> files, std::chrono::sys_seconds now) {
  SeedReport report;
  auto parsed = parseAll(files, report.files);
  if (!parsed) return tl::unexpected(parsed.error());

  try {
    Transaction tx{db};
    // Upsert NPCs. The WHERE guard means a seed id can only ever update an NPC row.
    auto upsertNpc = db.prepare(
        "INSERT INTO user(id, name, kind, role, settings, created_at, last_seen_at) "
        "VALUES (?, ?, 'NPC', 'USER', '{}', ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET name = excluded.name WHERE user.kind = 'NPC'");
    for (const auto& npc : parsed->npcs) {
      upsertNpc.bind(1, npc.id).bind(2, npc.name).bind(3, parsed->snapshotAt).bind(4, parsed->snapshotAt).run();
      upsertNpc.reset();
      ++report.npcsUpserted;
    }

    // Agents. A player's own Agent with the same slug wins; the seed Agent is skipped, along with its rows.
    std::vector<std::string> skipped;
    auto slugOwner = db.prepare("SELECT id FROM agent WHERE slug = ?");
    auto upsertAgent = db.prepare(
        "INSERT INTO agent(id, slug, name, vendor, kind, examine, about, website, status, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'APPROVED', ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET name = excluded.name, vendor = excluded.vendor, kind = excluded.kind, "
        "examine = excluded.examine, about = excluded.about, website = excluded.website, "
        "updated_at = excluded.updated_at");  // status is left alone: a Keeper's decision stands
    for (const auto& a : parsed->agents) {
      slugOwner.bind(1, a.slug);
      const bool taken = slugOwner.step() && slugOwner.columnText(0) != a.id;
      slugOwner.reset();
      if (taken) {
        skipped.push_back(a.slug);
        ++report.agentsSkipped;
        continue;
      }
      upsertAgent.bind(1, a.id)
          .bind(2, a.slug)
          .bind(3, a.input.name)
          .bind(4, a.input.vendor)
          .bind(5, a.input.kind == domain::AgentKind::Tool ? "TOOL" : "MODEL")
          .bind(6, a.input.examine)
          .bind(7, a.input.about)
          .bind(8, a.input.website)
          .bind(9, parsed->agentsAt)
          .bind(10, parsed->agentsAt)
          .run();
      upsertAgent.reset();
      ++report.agentsUpserted;
    }
    const auto isSkipped = [&](const std::string& slug) {
      return std::find(skipped.begin(), skipped.end(), slug) != skipped.end();
    };
    const auto agentId = [](const std::string& slug) { return std::string(SEED_ID_PREFIX) + "agent:" + slug; };

    // Rating baseline: swap only the seeded share of sum/count, so ratings players added are never lost.
    auto findStat = db.prepare(
        "SELECT seed_sum, seed_count FROM agent_skill_stat WHERE agent_id = ? AND skill = ? AND quest_type = ?");
    auto insertStat = db.prepare(
        "INSERT INTO agent_skill_stat(agent_id, skill, quest_type, sum, count, seed_sum, seed_count) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    auto swapStat = db.prepare(
        "UPDATE agent_skill_stat SET sum = sum - seed_sum + ?, count = count - seed_count + ?, "
        "seed_sum = ?, seed_count = ? WHERE agent_id = ? AND skill = ? AND quest_type = ?");
    for (const auto& s : parsed->stats) {
      if (isSkipped(s.agentSlug)) continue;
      const std::string id = agentId(s.agentSlug);
      findStat.bind(1, id).bind(2, s.stat).bind(3, s.questType);
      const bool exists = findStat.step();
      findStat.reset();
      if (exists) {
        swapStat.bind(1, s.sum).bind(2, s.count).bind(3, s.sum).bind(4, s.count).bind(5, id).bind(6, s.stat).bind(7, s.questType).run();
        swapStat.reset();
      } else {
        insertStat.bind(1, id).bind(2, s.stat).bind(3, s.questType).bind(4, s.sum).bind(5, s.count).bind(6, s.sum).bind(7, s.count).run();
        insertStat.reset();
      }
      ++report.statRows;
    }

    // Notes. Text and tags are refreshed; vote counts and the Keeper's hidden flag are left alone after the
    // first insert, because players' votes and moderation live in those columns.
    auto upsertNote = db.prepare(
        "INSERT INTO note(id, agent_id, author_id, kind, skill, version, text, agree, disagree, hidden, created_at) "
        "VALUES (?, ?, ?, ?, ?, NULL, ?, ?, ?, 0, ?) "
        "ON CONFLICT(id) DO UPDATE SET kind = excluded.kind, skill = excluded.skill, text = excluded.text");
    for (const auto& n : parsed->notes) {
      if (isSkipped(n.agentSlug)) continue;
      upsertNote.bind(1, n.id)
          .bind(2, agentId(n.agentSlug))
          .bind(3, n.authorId)
          .bind(4, n.kind)
          .bind(5, n.skill)
          .bind(6, n.text)
          .bind(7, n.agree)
          .bind(8, n.disagree)
          .bind(9, parsed->notesAt)
          .run();
      upsertNote.reset();
      ++report.notesUpserted;
    }

    setAppState(db, "seed_version", std::to_string(SEED_VERSION));
    setAppState(db, "seeded_at", core::formatIso8601(now));
    tx.commit();
  } catch (const std::exception& e) {
    return makeError(GameErrorCode::StorageFailed, "Something went wrong. Nothing was saved.", e.what());
  }
  return report;
}

}  // namespace as::db

#include "db/seed_loader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <optional>

#include "core/civil_time.hpp"

namespace as::db {

namespace {

using nlohmann::json;

struct NpcSeed {
  std::string id;
  std::string name;
};

struct ParsedSeed {
  std::string snapshotAt;  // ISO-8601, used as created_at/last_seen_at for NPC rows
  std::vector<NpcSeed> npcs;
};

// Files are applied in this order (later files reference earlier ones: quest logs need NPCs and
// Agents). New seed files are added here as their specs land (agent-codex: agents.json, notes.json;
// quest-logs: quest_logs.json).
constexpr std::array<std::string_view, 1> KNOWN_FILES{"npcs.json"};

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
    if (!r) return tl::unexpected(r.error());
    order.emplace_back(known);
  }
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
    setAppState(db, "seed_version", std::to_string(SEED_VERSION));
    setAppState(db, "seeded_at", core::formatIso8601(now));
    tx.commit();
  } catch (const std::exception& e) {
    return makeError(GameErrorCode::StorageFailed, "Something went wrong. Nothing was saved.", e.what());
  }
  return report;
}

}  // namespace as::db

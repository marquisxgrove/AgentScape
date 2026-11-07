#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "db/database.hpp"
#include "domain/codex_query.hpp"
#include "domain/compare.hpp"
#include "domain/graveyard.hpp"
#include "domain/stat_table.hpp"
#include "services/codex_cache.hpp"
#include "services/codex_store.hpp"

namespace as::services {

enum class NoteKind { Boon, Bane };

// A Boon or Bane as shown on an examine card. Only visible (not hidden) notes are ever returned.
struct NoteView {
  std::string id;
  std::string agentId;
  NoteKind kind = NoteKind::Boon;
  std::optional<std::string> skill;    // Skill storage name, e.g. "TOOL_USE"
  std::optional<std::string> version;
  std::string text;                    // plain text: render with TextUnformatted, never as markup
  std::int64_t agree = 0;
  std::int64_t disagree = 0;
  std::string authorId;
  std::string authorName;
  std::chrono::sys_seconds createdAt;
  double score = 0.0;                  // Wilson lower bound of agree / (agree + disagree)
};

struct CodexRow {
  Agent agent;
  domain::AgentLevels levels;  // in the requested Quest Type scope
};

struct CodexPage {
  std::vector<CodexRow> rows;          // this page only
  std::size_t total = 0;               // matches across all pages
  domain::PageSlice slice;
  std::vector<std::string> vendors;    // every vendor in the Codex, for the filter (unaffected by the other filters)
};

// One grave in the Hall of Hallucinations: a top-voted Bane and the Agent it belongs to.
struct Tombstone {
  std::string noteId;
  std::string agentSlug;
  std::string agentName;
  std::optional<std::string> skill;   // Skill storage name, e.g. "RELIABILITY"
  std::string text;                   // plain text: the epitaph
  std::int64_t agree = 0;
  std::int64_t disagree = 0;
};

struct AgentCard {
  Agent agent;
  domain::AgentLevels levels;
  std::vector<NoteView> boons;         // the top N by score
  std::vector<NoteView> banes;
  std::size_t boonTotal = 0;           // how many visible Boons / Banes exist in all
  std::size_t baneTotal = 0;
};

// Reads for the Codex screens (Requirements 1 and 2). Agents and their rating tables are loaded once and
// reused until the CodexCache generation moves, so paging, searching and switching Quest Type don't hit
// the database. Levels are computed in memory from the (sum, count) aggregates.
class CodexService {
 public:
  CodexService(db::Database& db, const CodexCache& cache) : db_(db), cache_(cache) {}

  // Approved Agents only. `scope` nullopt = all Quest Types.
  Result<CodexPage> list(const domain::ListQuery& query, std::optional<domain::QuestType> scope);

  // The card for an approved Agent, or nullopt if there is none by that slug (or it isn't approved).
  Result<std::optional<AgentCard>> card(const std::string& slug, std::optional<domain::QuestType> scope,
                                        std::size_t topNotes = 5);

  // The Hall of Hallucinations (Requirement 7.1): up to domain::MAX_TOMBSTONES visible Banes of approved Agents
  // with at least 3 votes, best Note_Score first.
  Result<std::vector<Tombstone>> tombstones();

  // Every approved Agent with its levels in `scope`, by name. For the Hiscores, which rank them.
  Result<std::vector<CodexRow>> allLevels(std::optional<domain::QuestType> scope);

  // Every approved Agent, by name (for pickers).
  Result<std::vector<Agent>> approvedAgents();

  // The levels of each named approved Agent (by slug) in `scope`, in the order given. A slug that is not an
  // approved Agent is skipped, so a stale selection can't break a comparison.
  Result<std::vector<domain::CompareColumn>> compareColumns(const std::vector<std::string>& slugs,
                                                            std::optional<domain::QuestType> scope);

  // Visible notes of one kind, best first (Wilson score, then newest). limit 0 = all.
  Result<std::vector<NoteView>> notes(const std::string& agentId, NoteKind kind, std::size_t limit = 0);

 private:
  struct Loaded {
    std::vector<Agent> agents;
    std::vector<domain::StatTable> tables;  // parallel to agents
  };
  Result<const Loaded*> loaded();

  db::Database& db_;
  const CodexCache& cache_;
  Loaded cached_;
  std::uint64_t cachedGeneration_ = 0;
};

const char* noteKindToDb(NoteKind kind);

}  // namespace as::services

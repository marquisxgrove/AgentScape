#include "domain/codex_query.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace as::domain {

namespace {
std::string lower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

std::vector<std::string> words(std::string_view s) {
  std::vector<std::string> out;
  std::string current;
  for (const char c : s) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!current.empty()) out.push_back(std::move(current));
      current.clear();
    } else {
      current += c;
    }
  }
  if (!current.empty()) out.push_back(std::move(current));
  return out;
}

bool matches(const Candidate& c, const std::vector<std::string>& terms) {
  if (terms.empty()) return true;
  const std::string haystack = lower(c.name) + " " + lower(c.vendor);
  return std::all_of(terms.begin(), terms.end(), [&](const std::string& t) { return haystack.find(t) != std::string::npos; });
}

// -1 / 0 / +1 comparison of two optional levels where "unranked" is handled by the caller.
int compareNumbers(std::int64_t a, std::int64_t b) { return a < b ? -1 : (a > b ? 1 : 0); }
}  // namespace

std::vector<std::size_t> selectAndSort(std::span<const Candidate> candidates, const ListQuery& q) {
  const auto terms = words(lower(q.text));
  const std::string vendorFilter = q.vendor ? lower(*q.vendor) : std::string{};

  std::vector<std::size_t> picked;
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    const Candidate& c = candidates[i];
    if (q.kind && c.kind != *q.kind) continue;
    if (q.vendor && lower(c.vendor) != vendorFilter) continue;
    if (!matches(c, terms)) continue;
    picked.push_back(i);
  }

  const auto skillIndex = static_cast<std::size_t>(std::find(SKILLS.begin(), SKILLS.end(), q.skill) - SKILLS.begin());
  const auto byName = [&](std::size_t a, std::size_t b) {
    const std::string na = lower(candidates[a].name);
    const std::string nb = lower(candidates[b].name);
    return na != nb ? (na < nb ? -1 : 1) : 0;
  };

  std::stable_sort(picked.begin(), picked.end(), [&](std::size_t a, std::size_t b) {
    const Candidate& ca = candidates[a];
    const Candidate& cb = candidates[b];
    int primary = 0;
    switch (q.sort) {
      case SortKey::Total: primary = compareNumbers(ca.levels.total, cb.levels.total); break;
      case SortKey::QuestLogs: primary = compareNumbers(ca.levels.overallCount, cb.levels.overallCount); break;
      case SortKey::Name: primary = byName(a, b); break;
      case SortKey::Skill: {
        const auto& la = ca.levels.skills[skillIndex];
        const auto& lb = cb.levels.skills[skillIndex];
        if (la.has_value() != lb.has_value()) return la.has_value();   // unranked last, in either direction
        if (la && lb) primary = compareNumbers(*la, *lb);
        break;
      }
    }
    if (primary != 0) {
      const bool naturalAscending = q.sort == SortKey::Name;           // names A-Z; numbers highest first
      const bool ascending = naturalAscending != q.reversed;
      return ascending ? primary < 0 : primary > 0;
    }
    const int tie = byName(a, b);   // ties: A to Z, regardless of direction
    return tie < 0;
  });
  return picked;
}

PageSlice paginate(std::size_t total, std::size_t page, std::size_t pageSize) {
  PageSlice s;
  if (pageSize == 0) pageSize = 1;
  s.pages = std::max<std::size_t>(1, (total + pageSize - 1) / pageSize);
  s.page = std::min(page, s.pages - 1);
  s.first = std::min(s.page * pageSize, total);
  s.count = std::min(pageSize, total - s.first);
  return s;
}

std::vector<std::string> distinctVendors(std::span<const Candidate> candidates) {
  std::set<std::string> seen;
  std::vector<std::string> out;
  for (const auto& c : candidates) {
    if (seen.insert(lower(c.vendor)).second) out.push_back(c.vendor);
  }
  std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
  return out;
}

}  // namespace as::domain

#include "domain/compare.hpp"

#include <algorithm>
#include <functional>

#include "domain/codex_validation.hpp"

namespace as::domain {

namespace {
CompareRow makeRow(std::string label, std::span<const CompareColumn> columns,
                   const std::function<std::optional<int>(const AgentLevels&)>& pick) {
  CompareRow row;
  row.label = std::move(label);
  std::optional<int> top;
  for (const auto& c : columns) {
    const auto v = pick(c.levels);
    row.levels.push_back(v);
    if (v && (!top || *v > *top)) top = v;
  }
  for (const auto& v : row.levels) row.best.push_back(v && top && *v == *top);
  return row;
}

std::size_t width(const std::string& s) { return utf8Length(s).value_or(s.size()); }

std::string padded(const std::string& s, std::size_t w) {
  std::string out = s;
  for (std::size_t n = width(s); n < w; ++n) out += ' ';
  return out;
}
}  // namespace

Comparison compare(std::span<const CompareColumn> columns, std::optional<QuestType> scope) {
  Comparison out;
  out.scopeLabel = scope ? displayName(*scope) : "All";
  for (const auto& c : columns) out.names.push_back(c.name);
  out.rows.push_back(makeRow("Overall", columns, [](const AgentLevels& l) { return l.overall; }));
  for (std::size_t i = 0; i < SKILLS.size(); ++i) {
    out.rows.push_back(makeRow(displayName(SKILLS[i]), columns, [i](const AgentLevels& l) { return l.skills[i]; }));
  }
  out.rows.push_back(makeRow("Total level", columns,
                             [](const AgentLevels& l) { return std::optional<int>{l.total}; }));
  return out;
}

std::string toPlainText(const Comparison& c) {
  const auto cell = [](const CompareRow& row, std::size_t i) {
    std::string s = row.levels[i] ? std::to_string(*row.levels[i]) : "-";
    if (row.best[i]) s += '*';
    return s;
  };
  std::size_t labelW = std::string("Skill").size();
  for (const auto& r : c.rows) labelW = std::max(labelW, width(r.label));
  std::vector<std::size_t> colW;
  for (std::size_t i = 0; i < c.names.size(); ++i) {
    std::size_t w = width(c.names[i]);
    for (const auto& r : c.rows) w = std::max(w, cell(r, i).size());
    colW.push_back(w);
  }

  std::string out = "Proving Grounds - Quest Type: " + c.scopeLabel + "\n";
  out += padded("Skill", labelW);
  for (std::size_t i = 0; i < c.names.size(); ++i) out += " | " + padded(c.names[i], colW[i]);
  out += "\n" + std::string(labelW, '-');
  for (const auto w : colW) out += "-+-" + std::string(w, '-');
  out += "\n";
  for (const auto& r : c.rows) {
    out += padded(r.label, labelW);
    for (std::size_t i = 0; i < c.names.size(); ++i) out += " | " + padded(cell(r, i), colW[i]);
    out += "\n";
  }
  out += "* = highest level in the row, - = unranked\n";
  return out;
}

}  // namespace as::domain

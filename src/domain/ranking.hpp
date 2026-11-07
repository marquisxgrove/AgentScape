#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace as::domain {

namespace detail {
// Names compare ignoring ASCII case first, then exactly, so the order is total and the same on every platform.
inline bool nameLess(std::string_view a, std::string_view b) {
  const std::size_t n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    const int x = std::tolower(static_cast<unsigned char>(a[i]));
    const int y = std::tolower(static_cast<unsigned char>(b[i]));
    if (x != y) return x < y;
  }
  if (a.size() != b.size()) return a.size() < b.size();
  return a < b;
}
}  // namespace detail

// Standard competition ranking (Requirement 7.3): highest score first, equal scores share a rank and the next
// rank skips ("1, 2, 2, 4"). Equal scores are ordered by name. `score(row)` is any ordered value; `name(row)` is
// a string. Returns each row with its rank, in display order.
template <class T, class Score, class Name>
std::vector<std::pair<T, int>> competitionRank(std::vector<T> rows, Score score, Name name) {
  std::stable_sort(rows.begin(), rows.end(), [&](const T& a, const T& b) {
    const auto sa = score(a);
    const auto sb = score(b);
    if (sa != sb) return sa > sb;
    return detail::nameLess(name(a), name(b));
  });
  std::vector<std::pair<T, int>> out;
  out.reserve(rows.size());
  int rank = 1;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    // A new score starts a new rank equal to 1 + the number of rows above it (that is, its position).
    // Read the score before the row is moved into the output.
    if (i > 0 && score(rows[i]) != score(out.back().first)) rank = static_cast<int>(i) + 1;
    out.emplace_back(std::move(rows[i]), rank);
  }
  return out;
}

}  // namespace as::domain

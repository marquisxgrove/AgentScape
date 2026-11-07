#include "domain/slug.hpp"

namespace as::domain {

std::string slugify(std::string_view name) {
  std::string out;
  out.reserve(name.size());
  bool pendingDash = false;
  for (const char raw : name) {
    char c = raw;
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (alnum) {
      if (pendingDash && !out.empty()) out += '-';
      pendingDash = false;
      out += c;
    } else {
      pendingDash = true;
    }
  }
  return out;
}

}  // namespace as::domain

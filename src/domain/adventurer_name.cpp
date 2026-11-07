#include "domain/adventurer_name.hpp"

#include <cctype>

namespace as::domain {

namespace {
bool isSeparator(char c) { return c == ' ' || c == '_' || c == '-'; }
bool isAlnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }
bool isWhitespace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::string_view trim(std::string_view s) {
  while (!s.empty() && isWhitespace(s.front())) s.remove_prefix(1);
  while (!s.empty() && isWhitespace(s.back())) s.remove_suffix(1);
  return s;
}
}  // namespace

std::string nameKey(std::string_view name) {
  std::string key;
  key.reserve(name.size());
  bool lastWasSpace = false;
  for (const char c : name) {
    if (isSeparator(c)) {
      if (!lastWasSpace && !key.empty()) {
        key += ' ';
        lastWasSpace = true;
      }
    } else {
      key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      lastWasSpace = false;
    }
  }
  if (!key.empty() && key.back() == ' ') key.pop_back();  // drop a trailing collapsed separator
  return key;
}

Result<std::string, NameError> validateName(std::string_view raw, std::span<const std::string_view> blocklist) {
  const std::string_view name = trim(raw);

  if (name.empty() || name.size() > 12) return tl::unexpected(NameError::Invalid);
  if (!isAlnum(name.front())) return tl::unexpected(NameError::Invalid);  // must start alnum (so no leading sep)
  if (isSeparator(name.back())) return tl::unexpected(NameError::Invalid);

  bool prevSep = false;
  for (const char c : name) {
    if (isAlnum(c)) {
      prevSep = false;
    } else if (isSeparator(c)) {
      if (prevSep) return tl::unexpected(NameError::Invalid);  // no consecutive separators
      prevSep = true;
    } else {
      return tl::unexpected(NameError::Invalid);  // disallowed character
    }
  }

  const std::string key = nameKey(name);
  for (const auto& blocked : blocklist) {
    if (nameKey(blocked) == key) return tl::unexpected(NameError::Blocked);
  }
  return std::string(name);
}

}  // namespace as::domain

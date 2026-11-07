#include "domain/codex_validation.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace as::domain {

namespace {
bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

std::string trimmed(std::string_view s) {
  while (!s.empty() && isSpace(s.front())) s.remove_prefix(1);
  while (!s.empty() && isSpace(s.back())) s.remove_suffix(1);
  return std::string(s);
}

bool hasControl(std::string_view s, bool allowNewline) {
  return std::any_of(s.begin(), s.end(), [&](char ch) {
    const auto c = static_cast<unsigned char>(ch);
    if (c == '\n' && allowNewline) return false;
    return c < 0x20 || c == 0x7f;
  });
}

tl::unexpected<FieldError> fail(std::string field, std::string message) {
  return tl::unexpected(FieldError{std::move(field), std::move(message)});
}

Result<std::string, FieldError> checkText(const std::string& field, std::string_view raw, std::size_t minLen,
                                          std::size_t maxLen, bool allowNewline) {
  std::string text = trimmed(raw);
  const auto len = utf8Length(text);
  if (!len || hasControl(text, allowNewline)) return fail(field, "That contains characters you can't use.");
  if (*len < minLen) return fail(field, minLen <= 1 ? "That can't be empty." : "That is too short.");
  if (*len > maxLen) return fail(field, "That is too long.");
  return text;
}
}  // namespace

std::optional<std::size_t> utf8Length(std::string_view s) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < s.size();) {
    const auto b0 = static_cast<unsigned char>(s[i]);
    std::size_t extra = 0;
    std::uint32_t cp = 0;
    if (b0 < 0x80) {
      cp = b0;
    } else if ((b0 & 0xE0) == 0xC0) {
      extra = 1;
      cp = b0 & 0x1F;
    } else if ((b0 & 0xF0) == 0xE0) {
      extra = 2;
      cp = b0 & 0x0F;
    } else if ((b0 & 0xF8) == 0xF0) {
      extra = 3;
      cp = b0 & 0x07;
    } else {
      return std::nullopt;
    }
    for (std::size_t k = 1; k <= extra; ++k) {
      if (i + k >= s.size()) return std::nullopt;  // truncated sequence
      const auto b = static_cast<unsigned char>(s[i + k]);
      if ((b & 0xC0) != 0x80) return std::nullopt;
      cp = (cp << 6) | (b & 0x3F);
    }
    const std::uint32_t minCp = extra == 0 ? 0 : (extra == 1 ? 0x80 : (extra == 2 ? 0x800 : 0x10000));
    if (cp < minCp) return std::nullopt;                       // overlong encoding
    if (cp > 0x10FFFF) return std::nullopt;                    // beyond Unicode
    if (cp >= 0xD800 && cp <= 0xDFFF) return std::nullopt;     // UTF-16 surrogates
    i += extra + 1;
    ++count;
  }
  return count;
}

Result<std::string, FieldError> validateWebsite(std::string_view url) {
  constexpr std::string_view kScheme = "https://";
  if (url.size() > MAX_WEBSITE) return fail("website", "That web address is too long.");
  if (url.substr(0, kScheme.size()) != kScheme) return fail("website", "Web addresses must start with https://");
  for (const char ch : url) {
    const auto c = static_cast<unsigned char>(ch);
    if (c <= 0x20 || c == 0x7f || c >= 0x80) return fail("website", "That web address has characters you can't use.");
  }
  const auto rest = url.substr(kScheme.size());
  const auto authorityEnd = rest.find_first_of("/?#");
  const auto authority = rest.substr(0, authorityEnd);
  if (authority.find('@') != std::string_view::npos) return fail("website", "Web addresses can't contain a login.");
  const auto colon = authority.find(':');
  const auto host = authority.substr(0, colon);
  if (colon != std::string_view::npos) {
    const auto port = authority.substr(colon + 1);
    if (port.empty() || port.size() > 5 || !std::all_of(port.begin(), port.end(), [](char c) { return c >= '0' && c <= '9'; })) {
      return fail("website", "That web address has a bad port.");
    }
  }
  const bool hostOk = !host.empty() && host.find('.') != std::string_view::npos && host.front() != '.' &&
                      host.back() != '.' && host.find("..") == std::string_view::npos &&
                      std::all_of(host.begin(), host.end(), [](char c) {
                        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.';
                      });
  if (!hostOk) return fail("website", "That doesn't look like a web address.");
  return std::string(url);
}

std::optional<std::string> websiteHost(std::string_view url) {
  if (!validateWebsite(url)) return std::nullopt;
  auto rest = url.substr(std::string_view{"https://"}.size());
  rest = rest.substr(0, rest.find_first_of("/?#"));
  rest = rest.substr(0, rest.find(':'));
  std::string host(rest);
  std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return host;
}

Result<AgentInput, FieldError> validateAgentInput(AgentInput raw) {
  AgentInput out;
  out.kind = raw.kind;

  auto name = checkText("name", raw.name, 1, MAX_AGENT_NAME, false);
  if (!name) return tl::unexpected(name.error());
  out.name = *name;

  auto vendor = checkText("vendor", raw.vendor, 1, MAX_AGENT_VENDOR, false);
  if (!vendor) return tl::unexpected(vendor.error());
  out.vendor = *vendor;

  auto examine = checkText("examine", raw.examine, 1, MAX_EXAMINE, false);
  if (!examine) return tl::unexpected(examine.error());
  out.examine = *examine;

  auto about = checkText("about", raw.about, 0, MAX_ABOUT, true);
  if (!about) return tl::unexpected(about.error());
  out.about = *about;

  if (raw.website) {
    const std::string site = trimmed(*raw.website);
    if (!site.empty()) {
      auto ok = validateWebsite(site);
      if (!ok) return tl::unexpected(ok.error());
      out.website = *ok;
    }
  }
  return out;
}

Result<std::string, FieldError> validateNoteText(std::string_view raw) {
  return checkText("text", raw, MIN_NOTE, MAX_NOTE, false);
}

}  // namespace as::domain

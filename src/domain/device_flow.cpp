#include "domain/device_flow.hpp"

#include <nlohmann/json.hpp>

namespace as::domain {

namespace {
using nlohmann::json;

std::optional<std::string> getString(const json& j, const char* key) {
  const auto it = j.find(key);
  if (it == j.end() || !it->is_string()) return std::nullopt;
  return it->get<std::string>();
}

// GitHub returns these numeric fields as JSON numbers; accept a string form too, defensively.
std::optional<long long> getSeconds(const json& j, const char* key) {
  const auto it = j.find(key);
  if (it == j.end()) return std::nullopt;
  if (it->is_number_integer()) return it->get<long long>();
  if (it->is_string()) {
    try {
      return std::stoll(it->get<std::string>());
    } catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

json parse(std::string_view s) { return json::parse(s.begin(), s.end(), nullptr, /*allow_exceptions=*/false); }
}  // namespace

std::optional<DeviceCode> parseDeviceCode(std::string_view jsonText) {
  const auto j = parse(jsonText);
  if (!j.is_object()) return std::nullopt;
  const auto device = getString(j, "device_code");
  const auto user = getString(j, "user_code");
  const auto uri = getString(j, "verification_uri");
  if (!device || !user || !uri) return std::nullopt;

  DeviceCode dc;
  dc.deviceCode = *device;
  dc.userCode = *user;
  dc.verificationUri = *uri;
  if (auto iv = getSeconds(j, "interval"); iv && *iv > 0) dc.interval = std::chrono::seconds{*iv};
  if (auto ex = getSeconds(j, "expires_in"); ex && *ex > 0) dc.expiresIn = std::chrono::seconds{*ex};
  return dc;
}

TokenPoll parseTokenPoll(std::string_view jsonText) {
  const auto j = parse(jsonText);
  if (!j.is_object()) return TokenError{"You can't reach GitHub right now."};

  if (auto token = getString(j, "access_token"); token && !token->empty()) {
    return TokenSuccess{*token};
  }
  const auto error = getString(j, "error");
  if (!error) return TokenError{"You can't reach GitHub right now."};
  if (*error == "authorization_pending") return TokenPending{};
  if (*error == "slow_down") return TokenSlowDown{};
  if (*error == "access_denied") return TokenDenied{};
  if (*error == "expired_token") return TokenExpired{};
  // invalid_client, unsupported_grant_type, incorrect_client_credentials, etc.
  return TokenError{"You can't reach GitHub right now."};
}

std::optional<GitHubProfile> parseProfile(std::string_view jsonText) {
  const auto j = parse(jsonText);
  if (!j.is_object()) return std::nullopt;
  const auto id = j.find("id");
  const auto login = getString(j, "login");
  if (id == j.end() || !id->is_number_integer() || !login) return std::nullopt;

  GitHubProfile p;
  p.id = id->get<std::int64_t>();
  p.login = *login;
  const auto name = getString(j, "name");  // may be JSON null or absent
  p.displayName = name && !name->empty() ? *name : *login;
  return p;
}

std::chrono::seconds nextPollInterval(std::chrono::seconds current, bool slowDown) {
  auto next = current;
  if (slowDown) next += std::chrono::seconds{5};
  if (next < std::chrono::seconds{1}) next = std::chrono::seconds{1};
  return next;
}

std::string_view deviceFlowMessage(const TokenPoll& poll) {
  if (std::holds_alternative<TokenDenied>(poll)) return "GitHub sign-in was cancelled.";
  if (std::holds_alternative<TokenExpired>(poll)) return "That code expired. Try again.";
  if (const auto* e = std::get_if<TokenError>(&poll)) return e->message;
  return {};
}

}  // namespace as::domain

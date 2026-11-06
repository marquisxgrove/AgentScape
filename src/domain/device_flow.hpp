#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace as::domain {

// Pure parsing and the poll-timing rule for the GitHub OAuth device flow (game-shell Req 2.4).
// All I/O (HTTP, threads, timers) lives in platform::github_device_flow and is desktop-only.

// POST /login/device/code response.
struct DeviceCode {
  std::string deviceCode;       // secret: used only to poll, never shown
  std::string userCode;         // shown to the user, e.g. "WDJB-MJHT"
  std::string verificationUri;  // e.g. https://github.com/login/device
  std::chrono::seconds interval{5};
  std::chrono::seconds expiresIn{900};
};

// Parsed outcome of one POST /login/oauth/access_token poll.
struct TokenPending {};                              // authorization_pending: keep polling
struct TokenSlowDown {};                             // slow_down: add 5s to the interval, keep polling
struct TokenDenied {};                               // access_denied: the user refused
struct TokenExpired {};                              // expired_token: the code timed out
struct TokenError { std::string message; };          // any other error (game-voice)
struct TokenSuccess { std::string accessToken; };    // authorization approved
using TokenPoll = std::variant<TokenPending, TokenSlowDown, TokenDenied, TokenExpired, TokenError, TokenSuccess>;

// GET /user profile (the only thing we read; the token is discarded afterwards).
struct GitHubProfile {
  std::int64_t id = 0;       // stable numeric id
  std::string login;         // handle
  std::string displayName;   // "name", falling back to login
};

// All parsers are lenient: malformed JSON or a missing required field yields nullopt / TokenError,
// never an exception.
std::optional<DeviceCode> parseDeviceCode(std::string_view json);
TokenPoll parseTokenPoll(std::string_view json);
std::optional<GitHubProfile> parseProfile(std::string_view json);

// interval after a response: base interval always applies; a slow_down adds 5 s (RFC 8628 / GitHub).
std::chrono::seconds nextPollInterval(std::chrono::seconds current, bool slowDown);

// User-facing (game-voice) message for a terminal poll result, matching design.md's error table.
std::string_view deviceFlowMessage(const TokenPoll& poll);

}  // namespace as::domain

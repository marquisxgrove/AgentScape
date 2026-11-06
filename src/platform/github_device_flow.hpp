#pragma once

#include <memory>
#include <optional>
#include <string>

#include "domain/device_flow.hpp"

namespace as::platform {

// Drives the GitHub OAuth device flow on a background thread (libcurl). Desktop only, and only when
// a client id is configured (AS_GITHUB_CLIENT_ID). The access token never leaves the worker: it is
// used once for GET /user, then discarded. Only the profile crosses back to the main thread.
//
// The flow is polled from the main thread with poll(); it never blocks. States mirror design.md:
//   Idle -> RequestingCode -> AwaitingUser -> Polling -> (Succeeded | Failed)

enum class DeviceFlowState { Idle, RequestingCode, AwaitingUser, Polling, Succeeded, Failed };

struct DeviceFlowSnapshot {
  DeviceFlowSnapshot() = default;
  explicit DeviceFlowSnapshot(DeviceFlowState s) : state(s) {}
  DeviceFlowSnapshot(DeviceFlowState s, std::string code, std::string uri,
                     std::optional<domain::GitHubProfile> prof, std::string err)
      : state(s), userCode(std::move(code)), verificationUri(std::move(uri)), profile(std::move(prof)),
        error(std::move(err)) {}

  DeviceFlowState state = DeviceFlowState::Idle;
  // Valid from AwaitingUser onward:
  std::string userCode;
  std::string verificationUri;
  // Valid when Succeeded:
  std::optional<domain::GitHubProfile> profile;
  // Valid when Failed (game-voice):
  std::string error;
};

// True only on a desktop build with a non-empty client id baked in.
bool githubSignInAvailable();

class GitHubDeviceFlow {
 public:
  GitHubDeviceFlow();
  ~GitHubDeviceFlow();
  GitHubDeviceFlow(const GitHubDeviceFlow&) = delete;
  GitHubDeviceFlow& operator=(const GitHubDeviceFlow&) = delete;

  // Begins (or restarts) the flow. No-op if sign-in is unavailable.
  void start();
  // Abandons any in-flight flow and returns to Idle.
  void cancel();
  // Thread-safe current state; call once per frame.
  DeviceFlowSnapshot poll();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Opens a URL in the system browser (SDL_OpenURL). Returns false on failure.
bool openUrl(const std::string& url);

}  // namespace as::platform

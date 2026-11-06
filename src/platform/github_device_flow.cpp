#include "platform/github_device_flow.hpp"

#include <SDL3/SDL.h>

namespace as::platform {

namespace {
const char* clientId() {
#ifdef AS_GITHUB_CLIENT_ID
  return AS_GITHUB_CLIENT_ID;
#else
  return "";
#endif
}
}  // namespace

bool openUrl(const std::string& url) {
  if (!SDL_OpenURL(url.c_str())) {
    SDL_Log("Could not open URL: %s", SDL_GetError());
    return false;
  }
  return true;
}

#if defined(AS_HAVE_CURL) && !defined(__EMSCRIPTEN__)

}  // namespace as::platform
#include "platform/github_device_flow_curl.inc"
namespace as::platform {

bool githubSignInAvailable() { return clientId()[0] != '\0'; }

#else  // no curl, or web build: sign-in is unavailable and the flow is inert.

// clientId() is read here so the "not built" path still references it; a non-empty id without a
// TLS backend (no curl) still can't sign in, so this is always false.
bool githubSignInAvailable() {
  (void)clientId();
  return false;
}

struct GitHubDeviceFlow::Impl {};
GitHubDeviceFlow::GitHubDeviceFlow() = default;
GitHubDeviceFlow::~GitHubDeviceFlow() = default;
void GitHubDeviceFlow::start() {}
void GitHubDeviceFlow::cancel() {}
DeviceFlowSnapshot GitHubDeviceFlow::poll() { return {}; }

#endif

}  // namespace as::platform

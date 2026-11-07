// libcurl-backed GitHub device flow. Included by github_device_flow.cpp only when AS_HAVE_CURL is
// set (desktop with a configured client id). Kept in a .inc so the curl headers never leak into the
// no-curl translation unit.
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "domain/device_flow.hpp"

namespace as::platform {

namespace {
constexpr char DEVICE_CODE_URL[] = "https://github.com/login/device/code";
constexpr char TOKEN_URL[] = "https://github.com/login/oauth/access_token";
constexpr char USER_URL[] = "https://api.github.com/user";
constexpr char SCOPE[] = "read:user";

std::size_t writeCb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  auto* out = static_cast<std::string*>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

struct HttpResult {
  bool ok = false;  // transport-level success (got any response)
  std::string body;
  long status = 0;  // HTTP status, when there was a response
};

// GitHub's OAuth "error" code from a response body ("incorrect_client_credentials", "device_flow_disabled", ...).
// Only this field is ever logged: a response body can hold a device code or an access token.
std::string githubError(const std::string& body) {
  const auto j = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object()) return "unreadable response";
  const auto it = j.find("error");
  return it != j.end() && it->is_string() ? it->get<std::string>() : std::string("no error field");
}

// The player only ever sees "You can't reach GitHub right now."; the log says which step failed and why.
void logFailure(const char* step, const HttpResult& r) {
  if (!r.ok) return;  // a transport failure was already logged by http()
  SDL_Log("GitHub sign-in: %s failed (HTTP %ld, %s)", step, r.status, githubError(r.body).c_str());
}

// Minimal POST/GET returning the body. Always asks for JSON. header is optional (e.g. auth).
HttpResult http(const std::string& url, const std::string& postFields, const std::string& bearer) {
  CURL* curl = curl_easy_init();
  if (curl == nullptr) return {};
  std::string body;
  curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/json");
  headers = curl_slist_append(headers, "User-Agent: AgentScape");
  std::string authHeader;
  if (!bearer.empty()) {
    authHeader = "Authorization: Bearer " + bearer;
    headers = curl_slist_append(headers, authHeader.c_str());
  }
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  if (!postFields.empty()) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
  }
  const CURLcode rc = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  if (rc != CURLE_OK) SDL_Log("GitHub sign-in: request to %s failed: %s", url.c_str(), curl_easy_strerror(rc));
  return {rc == CURLE_OK, std::move(body), status};
}

std::string formField(const std::string& k, const std::string& v) {
  char* ek = curl_easy_escape(nullptr, k.c_str(), 0);
  char* ev = curl_easy_escape(nullptr, v.c_str(), 0);
  std::string out = std::string(ek) + "=" + ev;
  curl_free(ek);
  curl_free(ev);
  return out;
}
}  // namespace

struct GitHubDeviceFlow::Impl {
  std::mutex mutex;
  DeviceFlowSnapshot snapshot;  // guarded by mutex
  std::thread worker;
  std::atomic<bool> cancel{false};
  std::atomic<bool> running{false};

  ~Impl() {
    cancel = true;
    if (worker.joinable()) worker.join();
  }

  void set(const DeviceFlowSnapshot& s) {
    std::lock_guard<std::mutex> lock(mutex);
    snapshot = s;
  }

  void run() {
    using namespace std::chrono;
    const std::string client = clientId();

    set(DeviceFlowSnapshot{DeviceFlowState::RequestingCode});
    auto codeResp = http(DEVICE_CODE_URL, formField("client_id", client) + "&" + formField("scope", SCOPE), "");
    auto code = codeResp.ok ? domain::parseDeviceCode(codeResp.body) : std::nullopt;
    if (!code) {
      logFailure("requesting a device code", codeResp);
      set({DeviceFlowState::Failed, "", "", std::nullopt, "You can't reach GitHub right now."});
      running = false;
      return;
    }

    {
      DeviceFlowSnapshot s{DeviceFlowState::AwaitingUser, code->userCode, code->verificationUri, std::nullopt, ""};
      set(s);
    }
    openUrl(code->verificationUri);

    auto interval = code->interval;
    const auto deadline = steady_clock::now() + code->expiresIn;
    const std::string pollFields = formField("client_id", client) + "&" + formField("device_code", code->deviceCode) +
                                   "&" + formField("grant_type", "urn:ietf:params:oauth:grant-type:device_code");

    while (!cancel.load()) {
      // Sleep in small slices so cancel() is responsive.
      for (auto waited = seconds{0}; waited < interval && !cancel.load(); waited += seconds{1}) {
        std::this_thread::sleep_for(seconds{1});
      }
      if (cancel.load()) break;
      if (steady_clock::now() >= deadline) {
        set({DeviceFlowState::Failed, "", "", std::nullopt, "That code expired. Try again."});
        running = false;
        return;
      }

      {
        std::lock_guard<std::mutex> lock(mutex);
        snapshot.state = DeviceFlowState::Polling;
      }
      auto tokenResp = http(TOKEN_URL, pollFields, "");
      const auto poll = tokenResp.ok ? domain::parseTokenPoll(tokenResp.body)
                                     : domain::TokenPoll{domain::TokenError{"You can't reach GitHub right now."}};

      if (const auto* success = std::get_if<domain::TokenSuccess>(&poll)) {
        // Use the token once, here, then let it die with this scope. Never store or log it.
        auto userResp = http(USER_URL, "", success->accessToken);
        auto profile = userResp.ok ? domain::parseProfile(userResp.body) : std::nullopt;
        if (profile) {
          set({DeviceFlowState::Succeeded, "", "", profile, ""});
        } else {
          if (userResp.ok) SDL_Log("GitHub sign-in: reading the profile failed (HTTP %ld)", userResp.status);
          set({DeviceFlowState::Failed, "", "", std::nullopt, "You can't reach GitHub right now."});
        }
        running = false;
        return;
      }
      if (std::holds_alternative<domain::TokenPending>(poll)) {
        continue;
      }
      if (std::holds_alternative<domain::TokenSlowDown>(poll)) {
        interval = domain::nextPollInterval(interval, /*slowDown=*/true);
        continue;
      }
      // Terminal error (denied, expired, or other).
      if (std::holds_alternative<domain::TokenError>(poll)) logFailure("polling for the token", tokenResp);
      set({DeviceFlowState::Failed, "", "", std::nullopt, std::string(domain::deviceFlowMessage(poll))});
      running = false;
      return;
    }
    running = false;
  }
};

GitHubDeviceFlow::GitHubDeviceFlow() : impl_(std::make_unique<Impl>()) {
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

GitHubDeviceFlow::~GitHubDeviceFlow() = default;

void GitHubDeviceFlow::start() {
  if (!githubSignInAvailable()) return;
  cancel();  // tear down any previous attempt
  impl_->cancel = false;
  impl_->running = true;
  impl_->set(DeviceFlowSnapshot{DeviceFlowState::RequestingCode});
  impl_->worker = std::thread([this] { impl_->run(); });
}

void GitHubDeviceFlow::cancel() {
  impl_->cancel = true;
  if (impl_->worker.joinable()) impl_->worker.join();
  impl_->cancel = false;
  impl_->set(DeviceFlowSnapshot{DeviceFlowState::Idle});
}

DeviceFlowSnapshot GitHubDeviceFlow::poll() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->snapshot;
}

}  // namespace as::platform

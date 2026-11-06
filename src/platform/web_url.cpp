#include "platform/web_url.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include <cstdlib>
#endif

namespace as::platform {

std::optional<std::string> urlQueryParam(const std::string& name) {
#ifdef __EMSCRIPTEN__
  // clang-format off
  char* raw = static_cast<char*>(EM_ASM_PTR({
    try {
      var v = new URLSearchParams(window.location.search).get(UTF8ToString($0));
      if (v === null) return 0;
      var len = lengthBytesUTF8(v) + 1;
      var p = _malloc(len);
      stringToUTF8(v, p, len);
      return p;
    } catch (e) { return 0; }
  }, name.c_str()));
  // clang-format on
  if (raw == nullptr) return std::nullopt;
  std::string value{raw};
  std::free(raw);
  return value;
#else
  (void)name;
  return std::nullopt;
#endif
}

}  // namespace as::platform

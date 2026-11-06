#include "platform/accessibility.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace as::platform {

bool prefersReducedMotion() {
#ifdef __EMSCRIPTEN__
  return EM_ASM_INT({
           try { return window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 1 : 0; }
           catch (e) { return 0; }
         }) != 0;
#else
  return false;
#endif
}

}  // namespace as::platform

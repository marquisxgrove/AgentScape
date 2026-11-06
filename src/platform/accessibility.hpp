#pragma once

namespace as::platform {

// Web: the browser's prefers-reduced-motion setting (game-shell Req 7.5), used as the default
// "Reduce motion" for a NEW user. Desktop: always false.
bool prefersReducedMotion();

}  // namespace as::platform

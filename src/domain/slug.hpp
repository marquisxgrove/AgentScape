#pragma once

#include <string>
#include <string_view>

namespace as::domain {

// Lowercase ASCII [a-z0-9-]: every other character (including non-ASCII bytes) is a separator; runs of
// separators collapse to one dash; leading and trailing dashes are trimmed. May be empty. Idempotent.
std::string slugify(std::string_view name);

}  // namespace as::domain

#pragma once

#include <optional>
#include <string>

namespace as::platform {

// Web: value of ?<name>= in the page URL. Desktop: always std::nullopt.
std::optional<std::string> urlQueryParam(const std::string& name);

}  // namespace as::platform

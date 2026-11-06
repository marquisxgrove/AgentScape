#include "platform/paths.hpp"

namespace as::platform {

std::filesystem::path resolveDataDir(const std::optional<std::string>& explicitDir,
                                     const std::function<std::filesystem::path()>& platformDefault) {
  if (explicitDir && !explicitDir->empty()) return std::filesystem::path{*explicitDir};
  return platformDefault();
}

}  // namespace as::platform

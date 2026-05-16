#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace Marginalia {

struct PackageDownloadInstallResult {
  bool ok = false;
  std::string packageId;
  std::string packageName;
  std::string error;
};

using PackageDownloadProgressCallback = std::function<void(size_t downloaded, size_t total)>;

PackageDownloadInstallResult downloadPackageArchiveToInbox(const std::string& url, const std::string& expectedSha256,
                                                           size_t expectedSize,
                                                           PackageDownloadProgressCallback progress = nullptr);

}  // namespace Marginalia

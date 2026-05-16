#pragma once

#include <string>

namespace Marginalia {

struct PackageArchiveInspectResult {
  bool ok = false;
  std::string packageId;
  std::string packageName;
  std::string version;
  std::string kind;
  std::string execution;
  std::string summary;
  std::string error;
};

struct PackageArchiveInstallResult {
  bool ok = false;
  std::string packageId;
  std::string packageName;
  std::string error;
};

PackageArchiveInspectResult inspectPackageArchive(const std::string& archivePath);
PackageArchiveInstallResult extractPackageArchiveToInbox(const std::string& archivePath);

}  // namespace Marginalia

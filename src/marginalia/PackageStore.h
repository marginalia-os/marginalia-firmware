#pragma once

#include <string>
#include <vector>

namespace Marginalia {

constexpr const char* PACKAGE_ROOT = "/.marginalia/packages";
constexpr const char* PACKAGE_INBOX_ROOT = "/.marginalia/inbox";
constexpr const char* PACKAGE_STAGING_ROOT = "/.marginalia/staging";
constexpr const char* PACKAGE_STATE_ROOT = "/.marginalia/package-state";
constexpr size_t MAX_MANIFEST_BYTES = 16384;

struct PackageManifest {
  std::string id;
  std::string directoryName;
  std::string name;
  std::string version;
  std::string kind;
  std::string execution;
  std::string summary;
  std::string author;
  std::string manifestPath;
  bool enabled = true;
  bool valid = false;
  std::string error;
};

class PackageStore {
 public:
  void scan();
  void scanInbox();
  const std::vector<PackageManifest>& packages() const { return packages_; }
  bool hadScanError() const { return hadScanError_; }
  PackageManifest readManifest(const std::string& packageDir, const std::string& packageDirName) const;

 private:
  std::vector<PackageManifest> packages_;
  bool hadScanError_ = false;

  void scanRoot(const char* rootPath);
};

bool isSafePackageId(const std::string& value);
bool isSafePackageRelativePath(const std::string& value);
bool ensurePackageBaseDirectories();
bool readPackageEnabled(const std::string& packageId);
bool setPackageEnabled(const std::string& packageId, bool enabled);
bool uninstallPackage(const std::string& packageId);

}  // namespace Marginalia

#pragma once

#include <string>
#include <vector>

namespace Marginalia {

constexpr const char* DEFAULT_PACKAGE_CATALOG_URL = "https://marginalia-hub.vercel.app/v1/catalog.json";

struct PackageCatalogEntry {
  std::string id;
  std::string name;
  std::string version;
  std::string kind;
  std::string summary;
  std::string url;
  std::string sha256;
  size_t size = 0;
  bool compatible = true;
  std::string compatibilityError;
};

struct PackageCatalogParseResult {
  bool ok = false;
  std::vector<PackageCatalogEntry> entries;
  std::string error;
};

PackageCatalogParseResult parsePackageCatalog(const std::string& json);

}  // namespace Marginalia

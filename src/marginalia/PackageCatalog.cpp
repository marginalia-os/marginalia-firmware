#include "PackageCatalog.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "CrossPointSettings.h"
#include "PackageStore.h"

namespace Marginalia {
namespace {

bool arrayContains(JsonVariantConst value, const char* expected) {
  if (!value.is<JsonArrayConst>()) return false;
  for (JsonVariantConst item : value.as<JsonArrayConst>()) {
    if (item.is<const char*>() && std::string(item.as<const char*>()) == expected) {
      return true;
    }
  }
  return false;
}

bool arrayContainsAny(JsonVariantConst value, const char* first, const char* second) {
  return arrayContains(value, first) || arrayContains(value, second);
}

bool parseVersionTriplet(const char* value, int (&parts)[3]) {
  if (value == nullptr || value[0] == '\0') return false;

  const char* cursor = value;
  for (int i = 0; i < 3; i++) {
    char* end = nullptr;
    const long parsed = strtol(cursor, &end, 10);
    if (end == cursor || parsed < 0 || parsed > 9999) {
      return false;
    }
    parts[i] = static_cast<int>(parsed);
    if (i < 2) {
      if (*end != '.') return false;
      cursor = end + 1;
    } else if (*end != '\0') {
      return false;
    }
  }
  return true;
}

bool firmwareMeetsMinimum(const char* minimum) {
  int current[3] = {0, 0, 0};
  int required[3] = {0, 0, 0};
  if (!parseVersionTriplet(CROSSPOINT_VERSION, current) || !parseVersionTriplet(minimum, required)) {
    return false;
  }

  for (int i = 0; i < 3; i++) {
    if (current[i] > required[i]) return true;
    if (current[i] < required[i]) return false;
  }
  return true;
}

std::string compatibilityError(JsonVariantConst target) {
  if (!target.is<JsonObjectConst>()) return "";

  JsonVariantConst devices = target["devices"];
  if (devices.is<JsonArrayConst>() && !arrayContainsAny(devices, "xteink-x3", "xteink-x4")) {
    return "unsupported device";
  }

  JsonVariantConst chipFamilies = target["chipFamilies"];
  if (chipFamilies.is<JsonArrayConst>() && !arrayContains(chipFamilies, "esp32-c3")) {
    return "unsupported chip family";
  }

  const int apiLevel = target["apiLevel"] | 1;
  if (apiLevel > PACKAGE_API_LEVEL) {
    return "requires newer package API";
  }

  if ((target["requiresPSRAM"] | false) == true) {
    return "requires PSRAM";
  }

  if (target["minFirmware"].is<const char*>() && !firmwareMeetsMinimum(target["minFirmware"].as<const char*>())) {
    return "requires newer firmware";
  }

  return "";
}

bool hasRequiredString(JsonObjectConst object, const char* key) {
  return object[key].is<const char*>() && object[key].as<const char*>()[0] != '\0';
}

PackageCatalogParseResult resultError(const std::string& error) {
  PackageCatalogParseResult result;
  result.error = error;
  return result;
}

}  // namespace

PackageCatalogParseResult parsePackageCatalog(const std::string& json) {
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json);
  if (error) {
    return resultError("catalog JSON is invalid");
  }
  if ((doc["schemaVersion"] | 0) != 1 || !doc["entries"].is<JsonArrayConst>()) {
    return resultError("catalog format is unsupported");
  }

  PackageCatalogParseResult result;
  for (JsonObjectConst item : doc["entries"].as<JsonArrayConst>()) {
    if (!hasRequiredString(item, "id") || !hasRequiredString(item, "name") || !hasRequiredString(item, "version") ||
        !hasRequiredString(item, "kind")) {
      continue;
    }

    JsonObjectConst artifact = item["artifact"];
    JsonObjectConst integrity = item["integrity"];
    if (!artifact["url"].is<const char*>()) {
      continue;
    }

    PackageCatalogEntry entry;
    entry.id = item["id"].as<const char*>();
    if (!isSafePackageId(entry.id)) {
      continue;
    }
    entry.name = item["name"].as<const char*>();
    entry.version = item["version"].as<const char*>();
    entry.kind = item["kind"].as<const char*>();
    entry.summary = item["summary"] | "";
    entry.url = artifact["url"].as<const char*>();
    entry.sha256 = integrity["sha256"] | "";
    entry.size = artifact["size"] | 0;
    entry.compatibilityError = compatibilityError(item["target"]);
    entry.compatible = entry.compatibilityError.empty();
    result.entries.push_back(std::move(entry));
  }

  std::stable_sort(result.entries.begin(), result.entries.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.compatible != rhs.compatible) return lhs.compatible;
    return lhs.name < rhs.name;
  });

  result.ok = true;
  return result;
}

}  // namespace Marginalia

#include "PackageThemeHost.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include "PackageStore.h"

namespace Marginalia {

namespace {

constexpr size_t MAX_THEME_BYTES = 2048;
constexpr const char* THEME_DESCRIPTOR = "src/theme.json";

bool loaded = false;
bool invertDisplay = false;
bool halfRefresh = false;
bool disableTextAntialiasing = false;
int readerCleanupInterval = 0;

std::string packageThemePath(const PackageManifest& package) {
  return std::string(PACKAGE_ROOT) + "/" + package.id + "/" + THEME_DESCRIPTOR;
}

struct ThemeDescriptor {
  bool invertDisplay = false;
  bool halfRefresh = false;
  bool disableTextAntialiasing = false;
  bool packageTextAntialiasingSetting = false;
  bool packageReaderRefreshSetting = false;
};

ThemeDescriptor readThemeDescriptor(const std::string& path) {
  ThemeDescriptor descriptor;
  FsFile file;
  if (!Storage.openFileForRead("MPKG", path.c_str(), file)) {
    return descriptor;
  }

  const size_t size = file.size();
  file.close();
  if (size == 0 || size > MAX_THEME_BYTES) {
    LOG_ERR("MPKG", "Theme descriptor has invalid size: %s", path.c_str());
    return descriptor;
  }

  const String json = Storage.readFile(path.c_str());
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("MPKG", "Theme descriptor parse failed: %s", path.c_str());
    return descriptor;
  }

  if ((doc["schemaVersion"] | 0) != 1 || !doc["scope"].is<const char*>() || !doc["mode"].is<const char*>()) {
    return descriptor;
  }
  if (std::string(doc["scope"].as<const char*>()) != "os") {
    return descriptor;
  }
  if (std::string(doc["mode"].as<const char*>()) == "invert-screen") {
    descriptor.invertDisplay = true;
  }
  if (doc["refreshMode"].is<const char*>() && std::string(doc["refreshMode"].as<const char*>()) == "half") {
    descriptor.halfRefresh = true;
  }
  if (doc["textAntialiasing"].is<const char*>() && std::string(doc["textAntialiasing"].as<const char*>()) == "off") {
    descriptor.disableTextAntialiasing = true;
  }
  if (doc["textAntialiasing"].is<const char*>() &&
      std::string(doc["textAntialiasing"].as<const char*>()) == "package-setting") {
    descriptor.packageTextAntialiasingSetting = true;
  }
  if (doc["readerRefresh"].is<const char*>() &&
      std::string(doc["readerRefresh"].as<const char*>()) == "package-setting") {
    descriptor.packageReaderRefreshSetting = true;
  }
  return descriptor;
}

int readerRefreshIntervalFromSetting(const std::string& value) {
  if (value == "every-page") return 1;
  if (value == "5-pages") return 5;
  if (value == "10-pages") return 10;
  return 0;
}

void reloadThemeState() {
  invertDisplay = false;
  halfRefresh = false;
  disableTextAntialiasing = false;
  readerCleanupInterval = 0;

  PackageStore store;
  store.scan();
  for (const auto& package : store.packages()) {
    if (!package.enabled || !package.compatible || package.kind != "theme") {
      continue;
    }

    const std::string path = packageThemePath(package);
    const ThemeDescriptor descriptor = readThemeDescriptor(path);
    if (descriptor.invertDisplay && readPackageSettingBool(package.id, "invertScreen", true)) {
      invertDisplay = true;
      halfRefresh = halfRefresh || descriptor.halfRefresh;
      disableTextAntialiasing = disableTextAntialiasing || descriptor.disableTextAntialiasing;
      if (descriptor.packageTextAntialiasingSetting) {
        disableTextAntialiasing =
            disableTextAntialiasing || !readPackageSettingBool(package.id, "textAntialiasing", false);
      }
      if (descriptor.packageReaderRefreshSetting) {
        const std::string refresh = readPackageSettingString(package.id, "readerRefresh", "every-page");
        const int interval = readerRefreshIntervalFromSetting(refresh);
        if (interval > 0 && (readerCleanupInterval == 0 || interval < readerCleanupInterval)) {
          readerCleanupInterval = interval;
        }
      }
      LOG_DBG("MPKG", "Enabled OS invert theme from package: %s", package.id.c_str());
    }
  }

  loaded = true;
}

}  // namespace

bool packageThemeInvertsDisplay() { return loaded && invertDisplay; }

bool packageThemeRequestsHalfRefresh() { return loaded && halfRefresh; }

bool packageThemeDisablesTextAntialiasing() { return loaded && disableTextAntialiasing; }

int packageThemeReaderCleanupInterval() { return loaded ? readerCleanupInterval : 0; }

void refreshPackageThemeHost() { reloadThemeState(); }

void markPackageThemeHostDirty() { loaded = false; }

}  // namespace Marginalia

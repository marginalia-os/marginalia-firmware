#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "marginalia/PackageCatalog.h"
#include "marginalia/PackageStore.h"
#include "util/ButtonNavigator.h"

class PackageHubActivity final : public Activity {
 public:
  enum class HubState { CHECK_WIFI, WIFI_SELECTION, LOADING, BROWSING, DOWNLOADING, INSTALLING, ERROR };

  explicit PackageHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("PackageHub", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  struct HubPackage {
    Marginalia::PackageCatalogEntry catalog;
    bool installed = false;
    std::string installedVersion;
  };

  ButtonNavigator buttonNavigator_;
  HubState state_ = HubState::CHECK_WIFI;
  std::vector<HubPackage> packages_;
  int selectedIndex_ = 0;
  std::string statusMessage_;
  std::string errorMessage_;
  size_t downloadProgress_ = 0;
  size_t downloadTotal_ = 0;
  bool wifiOwnedByActivity_ = false;

  void checkAndConnectWifi();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void loadCatalog();
  void refreshInstallState();
  void downloadAndInstallSelected();
  void setError(const std::string& message);
};

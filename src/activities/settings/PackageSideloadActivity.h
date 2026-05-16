#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "marginalia/PackageArchiveInstaller.h"
#include "util/ButtonNavigator.h"

class PackageSideloadActivity final : public Activity {
 public:
  enum class State { SCANNING, BROWSING, INSTALLING, ERROR };

  explicit PackageSideloadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("PackageSideload", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state_ == State::INSTALLING || state_ == State::SCANNING; }
  bool skipLoopDelay() override { return state_ == State::INSTALLING || state_ == State::SCANNING; }

 private:
  struct SideloadPackage {
    std::string path;
    std::string fileName;
    size_t size = 0;
    Marginalia::PackageArchiveInspectResult archive;
    bool installed = false;
    std::string installedVersion;
  };

  ButtonNavigator buttonNavigator_;
  State state_ = State::SCANNING;
  std::vector<SideloadPackage> packages_;
  int selectedIndex_ = 0;
  std::string errorMessage_;
  std::string statusMessage_;

  void scanArchives();
  void refreshInstallState();
  void installSelected();
  void setError(const std::string& message);
};

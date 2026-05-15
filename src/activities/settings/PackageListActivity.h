#pragma once

#include <string>
#include <vector>

#include "I18nKeys.h"
#include "activities/Activity.h"
#include "marginalia/PackageStore.h"
#include "util/ButtonNavigator.h"

class PackageListActivity final : public Activity {
 public:
  explicit PackageListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                               StrId title = StrId::STR_INSTALLED, std::string kindFilter = {});

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  Marginalia::PackageStore packageStore_;
  std::vector<Marginalia::PackageManifest> visiblePackages_;
  ButtonNavigator buttonNavigator_;
  int selectedIndex_ = 0;
  StrId title_;
  std::string kindFilter_;

  void refreshPackages();
};

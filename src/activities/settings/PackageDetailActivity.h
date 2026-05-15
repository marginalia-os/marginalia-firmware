#pragma once

#include <string>
#include <vector>

#include "I18nKeys.h"
#include "activities/Activity.h"
#include "marginalia/PackageStore.h"
#include "util/ButtonNavigator.h"

class PackageDetailActivity final : public Activity {
 public:
  PackageDetailActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Marginalia::PackageManifest package);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class RowAction {
    ToggleEnabled,
    Settings,
    None,
  };

  struct Row {
    StrId label;
    std::string value;
    RowAction action;
  };

  Marginalia::PackageManifest package_;
  std::vector<Row> rows_;
  ButtonNavigator buttonNavigator_;
  int selectedIndex_ = 0;

  void rebuildRows();
  void handleSelection();
};

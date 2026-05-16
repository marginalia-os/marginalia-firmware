#pragma once

#include <string>
#include <vector>

#include "I18nKeys.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class ExtensionsActivity final : public Activity {
 public:
  explicit ExtensionsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Extensions", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct ExtensionSection {
    StrId title;
    StrId subtitle;
    std::string kindFilter;
    bool opensHub = false;
  };

  std::vector<ExtensionSection> sections_;
  ButtonNavigator buttonNavigator_;
  int selectedIndex_ = 0;

  void openSelectedSection();
};

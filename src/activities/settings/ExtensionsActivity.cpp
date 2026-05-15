#include "ExtensionsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "PackageListActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void ExtensionsActivity::onEnter() {
  Activity::onEnter();
  sections_ = {
      {StrId::STR_INSTALLED, StrId::STR_EXTENSIONS_INSTALLED_DESC, ""},
      {StrId::STR_EXTENSION_THEMES, StrId::STR_EXTENSION_THEMES_DESC, "theme"},
      {StrId::STR_EXTENSION_READER, StrId::STR_EXTENSION_READER_DESC, "reader_module"},
      {StrId::STR_EXTENSION_SLEEP_SCREENS, StrId::STR_EXTENSION_SLEEP_SCREENS_DESC, "sleep_screen"},
      {StrId::STR_EXTENSION_INTEGRATIONS, StrId::STR_EXTENSION_INTEGRATIONS_DESC, "integration"},
      {StrId::STR_EXTENSION_EXPERIMENTAL_APPS, StrId::STR_EXTENSION_EXPERIMENTAL_APPS_DESC, "app"},
  };
  selectedIndex_ = 0;
  requestUpdate();
}

void ExtensionsActivity::onExit() {
  Activity::onExit();
  sections_.clear();
}

void ExtensionsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    activityManager.goHome();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    openSelectedSection();
    return;
  }

  const int sectionCount = static_cast<int>(sections_.size());
  if (sectionCount <= 0) return;

  const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, true);

  buttonNavigator_.onNextRelease([this, sectionCount] {
    selectedIndex_ = ButtonNavigator::nextIndex(selectedIndex_, sectionCount);
    requestUpdate();
  });

  buttonNavigator_.onPreviousRelease([this, sectionCount] {
    selectedIndex_ = ButtonNavigator::previousIndex(selectedIndex_, sectionCount);
    requestUpdate();
  });

  buttonNavigator_.onNextContinuous([this, sectionCount, pageItems] {
    selectedIndex_ = ButtonNavigator::nextPageIndex(selectedIndex_, sectionCount, pageItems);
    requestUpdate();
  });

  buttonNavigator_.onPreviousContinuous([this, sectionCount, pageItems] {
    selectedIndex_ = ButtonNavigator::previousPageIndex(selectedIndex_, sectionCount, pageItems);
    requestUpdate();
  });
}

void ExtensionsActivity::openSelectedSection() {
  if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(sections_.size())) return;

  const auto& section = sections_[selectedIndex_];
  auto resultHandler = [this](const ActivityResult&) { requestUpdate(); };
  startActivityForResult(
      std::make_unique<PackageListActivity>(renderer, mappedInput, section.title, section.kindFilter), resultHandler);
}

void ExtensionsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_EXTENSIONS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(sections_.size()), selectedIndex_,
      [this](int index) { return std::string(I18N[sections_[index].title]); },
      [this](int index) { return std::string(I18N[sections_[index].subtitle]); }, nullptr, nullptr, true);

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

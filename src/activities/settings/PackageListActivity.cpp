#include "PackageListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include "MappedInputManager.h"
#include "PackageDetailActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

PackageListActivity::PackageListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, StrId title,
                                         std::string kindFilter)
    : Activity("PackageList", renderer, mappedInput), title_(title), kindFilter_(std::move(kindFilter)) {}

void PackageListActivity::onEnter() {
  Activity::onEnter();
  selectedIndex_ = 0;
  refreshPackages();
  requestUpdate();
}

void PackageListActivity::onExit() { Activity::onExit(); }

void PackageListActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int packageCount = static_cast<int>(visiblePackages_.size());
  if (packageCount <= 0) return;

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    const auto& package = visiblePackages_[selectedIndex_];
    auto resultHandler = [this](const ActivityResult&) {
      refreshPackages();
      requestUpdate();
    };
    startActivityForResult(std::make_unique<PackageDetailActivity>(renderer, mappedInput, package), resultHandler);
    return;
  }

  const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, true);

  buttonNavigator_.onNextRelease([this, packageCount] {
    selectedIndex_ = ButtonNavigator::nextIndex(selectedIndex_, packageCount);
    requestUpdate();
  });

  buttonNavigator_.onPreviousRelease([this, packageCount] {
    selectedIndex_ = ButtonNavigator::previousIndex(selectedIndex_, packageCount);
    requestUpdate();
  });

  buttonNavigator_.onNextContinuous([this, packageCount, pageItems] {
    selectedIndex_ = ButtonNavigator::nextPageIndex(selectedIndex_, packageCount, pageItems);
    requestUpdate();
  });

  buttonNavigator_.onPreviousContinuous([this, packageCount, pageItems] {
    selectedIndex_ = ButtonNavigator::previousPageIndex(selectedIndex_, packageCount, pageItems);
    requestUpdate();
  });
}

void PackageListActivity::refreshPackages() {
  packageStore_.scan();
  visiblePackages_.clear();
  std::copy_if(packageStore_.packages().begin(), packageStore_.packages().end(), std::back_inserter(visiblePackages_),
               [this](const auto& package) { return kindFilter_.empty() || package.kind == kindFilter_; });
  if (!visiblePackages_.empty() && selectedIndex_ >= static_cast<int>(visiblePackages_.size())) {
    selectedIndex_ = static_cast<int>(visiblePackages_.size()) - 1;
  }
}

void PackageListActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& packages = visiblePackages_;

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, I18N[title_],
                 Marginalia::PACKAGE_ROOT);

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  if (packages.empty()) {
    const auto message = packageStore_.hadScanError() ? tr(STR_PACKAGE_SCAN_ERROR) : tr(STR_NO_PACKAGES);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, message);
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(packages.size()), selectedIndex_,
        [&packages](int index) { return packages[index].name; },
        [&packages](int index) {
          const auto& package = packages[index];
          if (!package.summary.empty()) return package.summary;
          return package.id;
        },
        nullptr,
        [&packages](int index) {
          const auto& package = packages[index];
          if (!package.compatible) return tr(STR_INCOMPATIBLE);
          return package.enabled ? tr(STR_ENABLED) : tr(STR_DISABLED);
        },
        true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_TOGGLE), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

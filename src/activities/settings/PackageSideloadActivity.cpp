#include "PackageSideloadActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cctype>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "marginalia/PackageStore.h"

namespace {

bool endsWithMpkgZip(const std::string& value) {
  constexpr const char* suffix = ".mpkg.zip";
  constexpr size_t suffixLen = 9;
  if (value.length() < suffixLen) return false;
  const size_t offset = value.length() - suffixLen;
  for (size_t i = 0; i < suffixLen; i++) {
    if (std::tolower(static_cast<unsigned char>(value[offset + i])) != suffix[i]) return false;
  }
  return true;
}

}  // namespace

void PackageSideloadActivity::onEnter() {
  Activity::onEnter();
  selectedIndex_ = 0;
  errorMessage_.clear();
  statusMessage_ = tr(STR_SCANNING);
  state_ = State::SCANNING;
  requestUpdate();
  scanArchives();
}

void PackageSideloadActivity::onExit() {
  Activity::onExit();
  packages_.clear();
}

void PackageSideloadActivity::loop() {
  if (state_ == State::SCANNING || state_ == State::INSTALLING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (state_ == State::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      scanArchives();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int packageCount = static_cast<int>(packages_.size());
  if (packageCount <= 0) return;

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    installSelected();
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

void PackageSideloadActivity::scanArchives() {
  state_ = State::SCANNING;
  statusMessage_ = tr(STR_SCANNING);
  requestUpdate(true);

  packages_.clear();
  if (!Marginalia::ensurePackageBaseDirectories()) {
    setError("Could not create package directories");
    return;
  }

  FsFile root = Storage.open(Marginalia::PACKAGE_SIDELOAD_ROOT);
  if (!root) {
    setError("Could not open sideload directory");
    return;
  }
  if (!root.isDirectory()) {
    root.close();
    setError("Sideload path is not a directory");
    return;
  }

  char nameBuffer[128];
  while (true) {
    FsFile entry = root.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) {
      entry.close();
      continue;
    }

    entry.getName(nameBuffer, sizeof(nameBuffer));
    const size_t size = entry.size();
    entry.close();

    std::string fileName = nameBuffer;
    if (!endsWithMpkgZip(fileName)) continue;

    SideloadPackage package;
    package.fileName = fileName;
    package.path = std::string(Marginalia::PACKAGE_SIDELOAD_ROOT) + "/" + fileName;
    package.size = size;
    package.archive = Marginalia::inspectPackageArchive(package.path);
    packages_.push_back(std::move(package));
  }
  root.close();

  std::stable_sort(packages_.begin(), packages_.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.archive.ok != rhs.archive.ok) return lhs.archive.ok;
    const std::string lhsName = lhs.archive.ok ? lhs.archive.packageName : lhs.fileName;
    const std::string rhsName = rhs.archive.ok ? rhs.archive.packageName : rhs.fileName;
    return lhsName < rhsName;
  });

  refreshInstallState();
  selectedIndex_ = 0;
  state_ = State::BROWSING;
  requestUpdate();
}

void PackageSideloadActivity::refreshInstallState() {
  Marginalia::PackageStore store;
  store.scan();
  for (auto& item : packages_) {
    item.installed = false;
    item.installedVersion.clear();
    if (!item.archive.ok) continue;
    const auto found = std::find_if(store.packages().begin(), store.packages().end(),
                                    [&item](const auto& package) { return package.id == item.archive.packageId; });
    if (found != store.packages().end()) {
      item.installed = true;
      item.installedVersion = found->version;
    }
  }
}

void PackageSideloadActivity::installSelected() {
  if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(packages_.size())) return;

  auto& selected = packages_[selectedIndex_];
  if (!selected.archive.ok) {
    setError(selected.archive.error.empty() ? "Invalid package archive" : selected.archive.error);
    return;
  }

  state_ = State::INSTALLING;
  statusMessage_ = selected.archive.packageName;
  requestUpdate(true);

  const auto extract = Marginalia::extractPackageArchiveToInbox(selected.path);
  if (!extract.ok) {
    setError(extract.error);
    return;
  }

  const auto install = Marginalia::installInboxPackage(extract.packageId);
  if (!install.ok) {
    setError(install.error);
    return;
  }

  LOG_DBG("MPKG", "Installed sideloaded package: %s", install.packageId.c_str());
  refreshInstallState();
  state_ = State::BROWSING;
  requestUpdate();
}

void PackageSideloadActivity::setError(const std::string& message) {
  errorMessage_ = message;
  state_ = State::ERROR;
  requestUpdate();
}

void PackageSideloadActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_EXTENSION_SD_CARD),
                 Marginalia::PACKAGE_SIDELOAD_ROOT);

  if (state_ == State::SCANNING || state_ == State::INSTALLING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2,
                              state_ == State::INSTALLING ? tr(STR_INSTALLING_PACKAGE) : statusMessage_.c_str());
    if (state_ == State::INSTALLING && !statusMessage_.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + renderer.getLineHeight(UI_10_FONT_ID),
                                statusMessage_.c_str());
    }
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state_ == State::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_ERROR_MSG));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, errorMessage_.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  if (packages_.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_NO_SD_PACKAGES));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(packages_.size()), selectedIndex_,
        [this](int index) {
          const auto& package = packages_[index];
          return package.archive.ok ? package.archive.packageName : package.fileName;
        },
        [this](int index) {
          const auto& package = packages_[index];
          if (!package.archive.ok) return package.archive.error;
          if (!package.archive.summary.empty()) return package.archive.summary;
          return package.archive.packageId;
        },
        nullptr,
        [this](int index) {
          const auto& package = packages_[index];
          if (!package.archive.ok) return tr(STR_ERROR_MSG);
          if (package.installed && package.installedVersion == package.archive.version) return tr(STR_INSTALLED);
          if (package.installed) return tr(STR_UPDATE);
          return tr(STR_SELECT);
        },
        true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

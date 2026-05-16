#include "PackageHubActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <WiFi.h>

#include <algorithm>
#include <iterator>
#include <memory>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "marginalia/PackageDownloadInstaller.h"
#include "network/HttpDownloader.h"

namespace {
constexpr int PROGRESS_BAR_HEIGHT = 20;
}

void PackageHubActivity::onEnter() {
  Activity::onEnter();
  state_ = HubState::CHECK_WIFI;
  selectedIndex_ = 0;
  packages_.clear();
  errorMessage_.clear();
  wifiOwnedByActivity_ = false;
  statusMessage_ = tr(STR_CHECKING_WIFI);
  requestUpdate();
  checkAndConnectWifi();
}

void PackageHubActivity::onExit() {
  Activity::onExit();
  if (wifiOwnedByActivity_) {
    WiFi.mode(WIFI_OFF);
    wifiOwnedByActivity_ = false;
  }
  packages_.clear();
}

void PackageHubActivity::loop() {
  if (state_ == HubState::WIFI_SELECTION) return;

  if (state_ == HubState::CHECK_WIFI || state_ == HubState::LOADING || state_ == HubState::DOWNLOADING ||
      state_ == HubState::INSTALLING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (state_ == HubState::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      checkAndConnectWifi();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (state_ != HubState::BROWSING) return;

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int packageCount = static_cast<int>(packages_.size());
  if (packageCount <= 0) return;

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    downloadAndInstallSelected();
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

void PackageHubActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_EXTENSION_HUB),
                 Marginalia::DEFAULT_PACKAGE_CATALOG_URL);

  if (state_ == HubState::CHECK_WIFI || state_ == HubState::LOADING || state_ == HubState::INSTALLING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage_.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state_ == HubState::DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 40, tr(STR_DOWNLOADING));
    auto title = renderer.truncatedText(UI_10_FONT_ID, statusMessage_.c_str(), pageWidth - 40);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 10, title.c_str());
    if (downloadTotal_ > 0) {
      GUI.drawProgressBar(renderer, Rect{50, pageHeight / 2 + 20, pageWidth - 100, PROGRESS_BAR_HEIGHT},
                          downloadProgress_, downloadTotal_);
    }
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state_ == HubState::ERROR) {
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
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_NO_HUB_PACKAGES));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(packages_.size()), selectedIndex_,
        [this](int index) { return packages_[index].catalog.name; },
        [this](int index) {
          const auto& item = packages_[index];
          if (!item.catalog.summary.empty()) return item.catalog.summary;
          return item.catalog.id;
        },
        nullptr,
        [this](int index) {
          const auto& item = packages_[index];
          if (!item.catalog.compatible) return tr(STR_INCOMPATIBLE);
          if (item.installed && item.installedVersion == item.catalog.version) return tr(STR_INSTALLED);
          if (item.installed) return tr(STR_UPDATE);
          return tr(STR_DOWNLOAD);
        },
        true);
  }

  const bool canInstall = !packages_.empty() && packages_[selectedIndex_].catalog.compatible;
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), canInstall ? tr(STR_DOWNLOAD) : "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void PackageHubActivity::checkAndConnectWifi() {
  state_ = HubState::CHECK_WIFI;
  statusMessage_ = tr(STR_CHECKING_WIFI);
  requestUpdate();
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    wifiOwnedByActivity_ = false;
    loadCatalog();
    return;
  }
  wifiOwnedByActivity_ = true;
  launchWifiSelection();
}

void PackageHubActivity::launchWifiSelection() {
  state_ = HubState::WIFI_SELECTION;
  requestUpdate();
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void PackageHubActivity::onWifiSelectionComplete(const bool connected) {
  if (!connected) {
    setError(tr(STR_WIFI_CONN_FAILED));
    return;
  }
  loadCatalog();
}

void PackageHubActivity::loadCatalog() {
  state_ = HubState::LOADING;
  statusMessage_ = tr(STR_LOADING_HUB_CATALOG);
  requestUpdate(true);

  std::string json;
  if (!HttpDownloader::fetchUrl(Marginalia::DEFAULT_PACKAGE_CATALOG_URL, json)) {
    setError(tr(STR_FETCH_HUB_CATALOG_FAILED));
    return;
  }

  const auto parsed = Marginalia::parsePackageCatalog(json);
  if (!parsed.ok) {
    setError(parsed.error);
    return;
  }

  packages_.clear();
  packages_.reserve(parsed.entries.size());
  std::transform(parsed.entries.begin(), parsed.entries.end(), std::back_inserter(packages_),
                 [](const auto& entry) { return HubPackage{entry, false, ""}; });
  refreshInstallState();
  selectedIndex_ = 0;
  state_ = HubState::BROWSING;
  requestUpdate();
}

void PackageHubActivity::refreshInstallState() {
  Marginalia::PackageStore store;
  store.scan();
  for (auto& item : packages_) {
    item.installed = false;
    item.installedVersion.clear();
    const auto found = std::find_if(store.packages().begin(), store.packages().end(),
                                    [&item](const auto& package) { return package.id == item.catalog.id; });
    if (found != store.packages().end()) {
      item.installed = true;
      item.installedVersion = found->version;
    }
  }
}

void PackageHubActivity::downloadAndInstallSelected() {
  if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(packages_.size())) return;

  const auto& selected = packages_[selectedIndex_].catalog;
  if (!selected.compatible) {
    setError(selected.compatibilityError.empty() ? tr(STR_INCOMPATIBLE) : selected.compatibilityError);
    return;
  }

  state_ = HubState::DOWNLOADING;
  statusMessage_ = selected.name;
  downloadProgress_ = 0;
  downloadTotal_ = selected.size;
  requestUpdate(true);

  const auto download = Marginalia::downloadPackageArchiveToInbox(selected.url, selected.sha256, selected.size,
                                                                  [this](const size_t downloaded, const size_t total) {
                                                                    downloadProgress_ = downloaded;
                                                                    downloadTotal_ = total;
                                                                    requestUpdate(true);
                                                                  });
  if (!download.ok) {
    setError(download.error);
    return;
  }

  state_ = HubState::INSTALLING;
  statusMessage_ = tr(STR_INSTALLING_PACKAGE);
  requestUpdate(true);

  const auto install = Marginalia::installInboxPackage(download.packageId);
  if (!install.ok) {
    setError(install.error);
    return;
  }

  refreshInstallState();
  state_ = HubState::BROWSING;
  requestUpdate();
}

void PackageHubActivity::setError(const std::string& message) {
  errorMessage_ = message;
  state_ = HubState::ERROR;
  requestUpdate();
}

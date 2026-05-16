#pragma once

namespace Marginalia {

bool packageThemeInvertsDisplay();
bool packageThemeRequestsHalfRefresh();
bool packageThemeDisablesTextAntialiasing();
int packageThemeReaderCleanupInterval();
void refreshPackageThemeHost();
void markPackageThemeHostDirty();

}  // namespace Marginalia

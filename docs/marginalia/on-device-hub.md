# On-Device Hub

The on-device Hub lives under **Extensions**. It is the device-side package browser, matching the same user model as
OPDS for books: connect to Wi-Fi, load a remote catalog, browse entries, download the selected item, then install it.

## Flow

1. Open **Settings** → **Extensions** → **Hub**.
2. If Wi-Fi is not connected, the firmware opens the existing Wi-Fi selection screen.
3. The Hub downloads the catalog from `https://marginalia-hub.vercel.app/v1/catalog.json`.
4. Catalog entries are parsed and sorted with compatible packages first.
5. Incompatible packages stay visible, but cannot be installed.
6. Selecting a compatible package downloads its `.mpkg.zip`, verifies size and SHA-256, extracts it into the inbox, and
   installs it through the normal package activation transaction.

## Error Handling

The Hub has explicit states for Wi-Fi selection, catalog loading, browsing, downloading, installing, and errors. Failed
catalog loads, invalid catalog JSON, download failures, checksum mismatches, incompatible packages, and install failures
all land on a retryable error screen instead of silently returning to Extensions.

## Install Contract

The Hub does not have its own installer. It uses the same archive download helper and `installInboxPackage()` transaction
as the web package screen. This keeps package replacement, rollback, and theme-host refresh behavior identical no matter
where the package came from.

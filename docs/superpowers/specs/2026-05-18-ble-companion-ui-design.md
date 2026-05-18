# BLE Companion UI Design

## Goal

Design a small user-facing companion UI for Marginalia BLE transfer without changing the firmware protocol or replacing
the Python CLI. The companion should make the existing BLE transfer service usable by people who do not want to run
terminal commands.

The first UI should support the stable operations already implemented by firmware:

- upload `.mpkg.zip` package archives
- upload `.epub` books
- upload `.bmp` images
- download `/crash_report.txt`
- download package-state diagnostics by package id
- authenticate with the visible six-digit code or a saved trusted-host record

This design is for a future PR. It does not implement the companion UI yet.

## Product Shape

Start with a browser-based Web Bluetooth companion. A native mobile app can come later, but a browser UI keeps the first
client easy to distribute, inspect, and update. The UI should be reachable from Marginalia docs or Hub, and it should run
as a static page without a backend.

Recommended placement:

- primary home: `marginalia-hub`, because Hub already owns user-facing catalog and web surfaces
- firmware repo: keep protocol documentation and cross-links only
- SDK repo: no changes unless shared manifest/package helpers become useful in browser code

The firmware repo should not grow a web client unless the page is only protocol documentation. Firmware owns the BLE
service and reader behavior; Hub should own the companion UI.

## Supported Browsers

Use Web Bluetooth where available:

- Chrome or Edge on desktop
- Android Chrome where platform support allows it

Safari and iOS browser support should be treated as unsupported for the first version. The page should detect missing
Web Bluetooth support and show a concise fallback that points users to the Python CLI. Do not introduce a separate
server bridge in the first version.

## User Flows

### Connect

1. User opens the companion page.
2. User opens **File Transfer > Bluetooth Transfer** on the reader.
3. User presses **Connect** in the companion.
4. Browser device picker filters for the Marginalia BLE Transfer service.
5. Companion connects, subscribes to `status`, and reads the initial status.
6. If a matching trusted-host record exists, companion tries nonce-based `hello`.
7. If trusted auth fails or no record exists, companion asks for the visible six-digit code.
8. Companion sends code-based `hello`.

The UI should not ask for a code before trying a saved trusted host. Code entry is the fallback, not the default path.

### Upload

1. User chooses Package, Book, or BMP.
2. User selects one local file.
3. Companion validates the suffix and safe filename before connecting or starting a transfer.
4. Companion computes SHA-256 in the browser.
5. Companion sends `start_put` with the existing kind:
   - `package`
   - `book`
   - `bmp`
6. Companion streams numbered chunks to `data-in`.
7. Companion shows progress from firmware `status`.
8. Companion sends `commit`.
9. Companion waits for `installed` or `saved`.
10. If the firmware returns a pairing prompt after a code-authenticated upload, companion shows **Save this computer?**.

Safe filename validation must match the firmware rules documented in
`docs/future-ideas/bluetooth-file-transfer.md`: basename only, no leading `.`, ASCII letters/digits plus `.`, `_`, and
`-`, extension-specific suffixes, and the firmware filename length limit.

Package uploads should clearly separate "uploaded" from "installed". Book and BMP uploads finish at "saved".
The **Save this computer?** prompt stays suppressed until firmware reports a final `installed` or `saved` state.

### Download Diagnostics

1. User chooses Crash Report or Package State.
2. For package state, user enters a package id.
3. Companion sends `start_get`.
4. Firmware sends one `data-out` frame.
5. Companion validates sequence and sends `get_ack`.
6. Companion repeats until final `sent`.
7. Companion saves a local file using the browser download API.

Downloads must not offer arbitrary path input. The UI only exposes the two firmware allowlisted download kinds.

### Browser Close

During an active `start_put` or `start_get` transfer, the companion should register a `beforeunload` handler so the
browser warns before tab or window closure. If the user closes anyway, the BLE connection is expected to drop.

For uploads, firmware owns incomplete `.part` cleanup on disconnect or transfer reset. The companion should not mark the
operation complete unless firmware reports `installed` or `saved`.

For downloads, the companion should discard any incomplete local blob unless firmware reports final `sent`. Resume is out
of scope for the first companion UI, so a later reconnect starts a fresh download.

## Protocol Mapping

The companion uses the existing Marginalia BLE Transfer GATT service:

- `control`: write with response; JSON commands
- `data-in`: upload chunk writes
- `data-out`: download notifications
- `status`: read/notify; state, progress, errors, nonce, trusted-host metadata

The companion should keep a small protocol client module with these responsibilities:

- scan and connect by service UUID
- subscribe to `status`
- send `hello`
- send `start_put`, data frames, `commit`, and `cancel`
- send `start_get` and `get_ack`
- normalize firmware statuses into UI states
- handle reconnect and disconnect cleanup

Do not fork the protocol for the browser. Any behavior that differs from `scripts/ble_transfer.py` should be treated as
a bug unless Web Bluetooth imposes a clear limitation.

## Trusted-Host Storage

The browser should store trusted-host records locally, similar to the Python CLI:

- device id
- host id
- host display name
- shared secret

Use browser-local storage that does not require a backend account. The first version can use IndexedDB or localStorage;
IndexedDB is preferred if the Hub stack already has a small storage helper.

Security notes:

- the secret is scoped to the browser profile and origin
- browser-local storage does not provide encryption at rest; stored secrets rely on OS file permissions and browser
  profile isolation
- anyone with access to the computer and browser profile may be able to read stored trusted-host secrets
- clearing browser site data removes trusted-host auth
- the UI must keep the visible-code fallback
- the UI should expose a **Forget saved reader** action

Do not sync trusted-host records through Hub or any cloud service.

## UI Structure

The first screen should be a compact transfer tool, not a marketing page.

Core sections:

- connection state: disconnected, scanning, connected, authenticated, transfer active
- authentication: trusted-host attempt, six-digit code fallback, save-host prompt
- upload actions: Package, Book, BMP
- diagnostic downloads: Crash Report, Package State
- progress panel: bytes, state, error, final destination
- saved reader management: device name/id, forget action

The UI should stay usable on a phone viewport. Use native file pickers and browser download behavior. Keep copy short and
operational; avoid explaining BLE internals in the main interface.

## Error Handling

The companion should map firmware errors to user-facing messages while preserving the raw error for debugging.

Important cases:

- no Web Bluetooth support
- no device selected
- device not advertising
- connection lost
- invalid or expired code
- trusted-host auth rejected
- unsafe filename
- unsupported file kind
- file already exists
- size mismatch
- SHA-256 mismatch
- package install failed
- download sequence mismatch
- browser/tab closed during an active transfer

On transfer failure, companion should send `cancel` if still connected. If the connection is already gone, the firmware
is responsible for `.part` cleanup on disconnect/reset, matching the existing protocol.

## Accessibility And Compatibility

The UI should:

- work without drag and drop
- expose file inputs as normal controls
- keep progress text readable by screen readers
- avoid relying on color alone for transfer state
- keep buttons disabled while an incompatible transfer is active
- show exact filenames and final destinations after success

The page should not require installing a PWA, but it can be installable later if Hub already supports that pattern.

## Testing Plan

Browser-level tests:

- protocol-client unit tests with mocked characteristics
- SHA-256 and frame-building tests
- filename validation tests matching firmware rules
- status-to-UI-state tests
- trusted-host HMAC tests using fixed nonce and secret vectors

Manual Web Bluetooth tests:

- first upload with visible code
- accept **Save Host?**, then reconnect without code
- decline or clear browser storage and verify code fallback
- upload package, EPUB, and BMP
- download crash report
- download package-state diagnostics
- verify duplicate upload error
- verify disconnect during upload recovers cleanly

Firmware regression tests remain in the firmware repo. The companion should not require firmware changes for its first
version.

## Rollout

Recommended implementation order:

1. Add a static companion page in Hub behind a clear "experimental" label.
2. Implement connect, status subscription, and code auth.
3. Implement trusted-host auth and local storage.
4. Implement package/book/BMP uploads using the existing protocol.
5. Implement diagnostic downloads.
6. Add docs links from firmware transfer docs and Hub.
7. Use real hardware validation before removing the experimental label.

## Out Of Scope

- arbitrary SD-card browsing
- arbitrary BLE file reads or writes
- background BLE advertising
- cloud sync for trusted-host secrets
- iOS support claims
- OS BLE bonding as the primary trust model
- resumable transfers
- BLE OTA

Resume and OTA need their own firmware protocol work before the UI exposes them.

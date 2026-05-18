# Bluetooth File Transfer

## Idea

Add a Bluetooth LE transfer mode for sending books, package archives, and diagnostic files between a phone or computer
and the reader without joining Wi-Fi or removing the SD card.

This should not try to expose the SD card as a mounted filesystem. It should be an application-level transfer service
that writes only to approved SD-card destinations and then hands package archives to the normal Marginalia installer.

## Why It Matters

BLE is more pleasant than USB serial for normal users:

- phones can connect without cables
- transfer can be initiated from a companion app or web app
- the reader can show an explicit pairing/transfer screen
- uploads can land directly in known places such as `/Books/` or `/.marginalia/sideload/`
- crash reports can be pulled after a failure without Wi-Fi setup

It is not a replacement for Wi-Fi for large libraries. It is a low-friction path for a few books, package archives, and
diagnostics.

## Hardware And Stack Findings

ESP32-C3 supports Bluetooth LE and Espressif documents both Bluedroid and ESP-NimBLE support on the chip. ESP-NimBLE
requires less heap and flash than Bluedroid, which matters on this firmware because Wi-Fi/TLS and reader code already
run close to memory limits.

The ESP32-C3 Bluetooth LE radio supports 1 Mbps PHY, 2 Mbps PHY, coded PHY for longer range, and LE Data Packet Length
Extension. Espressif's BLE data-exchange docs model the right shape for this feature: a custom GATT service with
read/write characteristics and server notifications or indications.

Context7 lookup also points to NimBLE-Arduino as the practical Arduino-facing stack. Its GATT server API supports custom
services, writable characteristics, encrypted writes, and notifications. That is enough for a framed transfer protocol.

## Current State

The first implementation is now split across focused PRs and uses a custom Marginalia BLE Transfer GATT service under
**File Transfer > Bluetooth Transfer**.

Implemented pieces:

- `control`: write with response; starts sessions, commits uploads, cancels transfers, requests approved downloads
- `data-in`: write/write-without-response; host sends upload chunks
- `data-out`: notify; reader sends download chunks
- `status`: read/notify; current state, progress, errors, session nonce, and trusted-host metadata
- visible six-digit code authentication for first use and recovery
- trusted-host authentication with a nonce-based HMAC challenge response
- **Save Host?** after successful authenticated uploads
- **Forget Host** from the Bluetooth Transfer screen
- host-side trusted-device config in `~/.config/marginalia/ble_hosts.json` with `0600` permissions
- firmware trusted-host storage on SD card with hardware-tied secret obfuscation
- `.mpkg.zip` package upload and install with `scripts/ble_transfer.py put-package`
- `.epub` book upload to `/Books/` with `scripts/ble_transfer.py put-book`
- `.bmp` image upload to `/Pictures/` with `scripts/ble_transfer.py put-bmp`
- firmware upload, validation, physical confirmation, and OTA flash with `scripts/ble_transfer.py put-firmware`
- windowed upload flow control where the host negotiates an ACK byte interval and waits for receiver progress before
  sending the next burst
- resumable uploads for package, EPUB, BMP, and firmware transfers with the host CLI `--resume` option
- resumable `/crash_report.txt` download with `scripts/ble_transfer.py get-crash-report --resume`
- resumable package state download with `scripts/ble_transfer.py get-package-state <package-id> --resume`
- advertising restart after failed or unauthenticated sessions, so the same Bluetooth Transfer screen can accept a
  follow-up code-based retry without being reopened

Download notifications originally arrived out of order on hardware, so crash-report download now uses stop-and-go ACKs:
the device sends one numbered `data-out` frame, the host validates it, then the host sends `get_ack` before the next
frame is emitted. This is slower than raw notification streaming but avoids dropped or reordered BLE notifications.

## Protocol Shape

The Marginalia BLE Transfer GATT service has four characteristics:

- `control`: write with response; starts sessions, commits files, cancels transfers, requests file reads
- `data-in`: write without response; phone/computer sends upload chunks
- `data-out`: notify; reader sends download chunks or progress events
- `status`: read/notify; current state, progress, error code, expected next chunk

Upload flow:

1. User opens **File Transfer > Bluetooth Transfer**.
2. Device advertises `Marginalia Transfer` for a limited window and keeps auto-sleep disabled.
3. Client connects and authenticates with the visible code or trusted-host challenge response.
4. Client sends `start_put` with destination class, filename, byte size, SHA-256, and optional upload ACK interval.
5. Firmware opens a `.part` file under a staging directory. If the client requests resume and a matching partial file is
   present, firmware hashes the existing prefix and reports the byte offset to continue from.
6. Client streams numbered chunks to `data-in` up to the negotiated window.
7. Firmware acknowledges receiver progress through `status`; the client waits for that progress before sending the next
   window.
8. Client sends `commit`.
9. Firmware verifies byte count and SHA-256, renames into the final approved path, then installs packages or validates
   and physically confirms firmware updates.

Download flow:

1. User opens **File Transfer > Bluetooth Transfer**.
2. Device advertises `Marginalia Transfer` for a limited window and keeps auto-sleep disabled.
3. Client connects and authenticates with the visible code or trusted-host challenge response.
4. Client sends `start_get` for an approved diagnostic kind such as `crash_report` or `package_state`, plus optional
   `offset` and `chunk_size` values for resuming a partial output file.
5. Firmware validates the offset against the allowlisted file size, seeks to that offset, and sends one numbered chunk
   over `data-out`.
6. Client validates the sequence and sends `get_ack`.
7. Firmware sends the next chunk, repeating until complete.
8. Firmware publishes final `sent` status.

If authentication fails and the client disconnects, the firmware clears the accepted session state, rotates the
trusted-host nonce, returns to `ADVERTISING`, and restarts BLE advertising while the Bluetooth Transfer screen remains
open. This keeps the visible code fallback usable after a stale trusted-host record or a mistyped code.

Upload resume is implemented for interrupted package, EPUB, BMP, and firmware writes when the client opts in with
`--resume`. The resume offset is based on the existing `.part` file size and the original chunk size. Diagnostic
download resume is also opt-in with `--resume`; the host keeps `<output>.part`, sends its size as `offset`, appends new
frames, and only renames the final output after the byte count matches firmware's reported file size. Windowed uploads use
write-without-response plus receiver progress ACKs by default; `--transfer-mode response` remains available for
conservative debugging, and raw `--transfer-mode no-response` is only for explicit speed experiments.

## Approved Destinations

Current write support is narrow:

- `/.marginalia/sideload/<safe-name>.mpkg.zip`
- `/Books/<safe-name>.epub`
- `/Pictures/<safe-name>.bmp`
- `/.marginalia/ota/firmware.bin`

Current read support is narrow:

- `/crash_report.txt`
- `/.marginalia/package-state/<safe-package-id>.json`

Do not provide arbitrary SD-card read/write over BLE. It creates security and corruption risk without much user value.

## Security And UX

BLE transfer mode should be explicit and temporary. The device should only advertise while the user is physically on the
Bluetooth Transfer screen.

Pairing should follow the Wi-Fi credential model already used by the firmware:

- first use requires the visible six-digit transfer code
- after a successful authenticated transfer, prompt **Save this host?**
- save a trusted-host record with a host id, display name, and shared secret
- store the shared secret with the same hardware-tied obfuscation pattern used for saved Wi-Fi passwords
- let later transfers authenticate with a nonce-based challenge response instead of asking for the code again
- show the trusted host name while connected
- provide **Forget host** with the same cancel/forget prompt shape as saved Wi-Fi networks
- keep the visible code fallback for new hosts, deleted host config, and recovery tooling

Use application-level trust before OS BLE bonding. NimBLE bonding and encrypted characteristics can be added later, but
the first paired-host flow should be testable through Bleak on macOS, Linux, and phones without relying on platform-
specific bonding behavior.

The protocol treats disconnects as normal. Non-resumable incomplete uploads are removed on disconnect. Resumable uploads
keep the `.part` file and restart advertising so the same host can reconnect and continue from the firmware-reported
offset. A richer resumable manifest for downloads and cross-client resume is still future work.

## Remaining Work

Useful follow-up PRs, in recommended order:

1. Richer resumable transfer manifests.
   Basic `.part`-based resume now covers uploads and the approved diagnostic downloads. Add richer manifests only if
   large files, cross-client resume, or phone clients make stronger interruption recovery necessary.
2. OS BLE bonding or encrypted characteristics.
   Application-level trust is enough for the current CLI and is easier to test across macOS, Linux, and phones. Bonding
   can be layered on later if the phone UI needs platform-native trust.
3. Phone or web companion UI.
   Build a user-facing client only after the Python CLI protocol has the intended Bluetooth feature surface. The UI
   should use the same code/trusted-host model rather than introducing a second pairing concept.

## Future Transport Options

### Is There A Bluetooth Analogue Of WebSockets?

Yes, but there are two different answers depending on how close the match needs to be.

The current Marginalia GATT service already has the practical WebSocket shape:

- a persistent BLE connection while **Bluetooth Transfer** is open;
- client-to-device commands through `control`;
- host-to-device data through `data-in`;
- device-to-host pushed status through `status` notifications;
- device-to-host data through `data-out` notifications.

That is enough for command/status streaming and small-to-medium file transfer. It is not literally a socket, because GATT
is still an attribute protocol with characteristic reads, writes, and notifications.

The closer BLE equivalent is L2CAP Connection Oriented Channels. Espressif documents L2CAP CoC as reliable,
stream-oriented, bidirectional, flow-controlled, and useful for large file transfers. ESP-IDF includes NimBLE central and
peripheral examples. ESP-IoT-Solution also documents ESP32-C3 support and a configurable MTU up to 512 bytes.

Implementation difficulty:

- GATT tuning: low to medium. The firmware already sets MTU to 517 and uses windowed flow control. Next tuning would be
  connection parameter updates, data length updates, and explicit 2M PHY requests where the central accepts them.
- L2CAP CoC: medium to high. It is the cleanest BLE socket-like direction, but it likely means dropping below the
  NimBLE-Arduino GATT wrapper into lower-level NimBLE APIs, enabling L2CAP CoC buffers, adding a new host implementation,
  and testing platform support. It is probably not a quick patch.
- BLE-to-Wi-Fi handoff: medium in firmware, high in client UX. The ESP32-C3 supports BLE and 2.4 GHz Wi-Fi with internal
  coexistence, and ESP-IDF's provisioning manager shows the same pattern: BLE or SoftAP transport, secure protocomm
  endpoints, optional custom endpoints, then application-specific behavior. For Marginalia, BLE could authenticate and
  negotiate a temporary SoftAP session, then the existing HTTP/WebSocket upload path could move large files over Wi-Fi.

### Fast File Transfer Direction

Pure BLE can be improved, but it should not be treated as the fastest long-term transport. Espressif's BLE 5 throughput
example shows roughly 175 KB/s with 2M PHY and MTU 517 under favorable conditions. That is enough for package archives,
books, diagnostics, and emergency firmware updates, but a multi-megabyte firmware image will still feel slow.

For genuinely fast local transfer, prefer a hybrid mode:

1. User opens **File Transfer > Bluetooth Transfer**.
2. Host authenticates over the existing BLE trust/code flow.
3. Firmware starts a temporary SoftAP with a short-lived random WPA2 password.
4. Firmware sends the SoftAP SSID, password, and upload URL over the authenticated BLE session.
5. The host joins the temporary network and uploads over the existing HTTP/WebSocket file-transfer surface.
6. Firmware stops the SoftAP and returns to the normal transfer screen when the session finishes or times out.

This maps better to large files because it keeps BLE for discovery, authentication, and control while using Wi-Fi for
bulk bytes. It also reuses the web server's existing high-throughput upload code instead of creating another large binary
transport.

Guardrails for any hybrid implementation:

- keep BLE advertising explicit and screen-scoped;
- require the existing code/trusted-host authentication before revealing temporary Wi-Fi credentials;
- use short-lived, randomly generated SoftAP credentials;
- never expose arbitrary SD-card reads or writes;
- reuse approved destination classes and existing package/firmware validation;
- show clear device UI for "waiting for Wi-Fi upload", progress, completion, timeout, and cancellation.

### RT-Thread Comparison

RT-Thread's `ota_downloader` does not appear to provide a Bluetooth WebSocket-like transport. Its OTA model is still
useful as a design reference because it keeps the destination explicit: HTTP/HTTPS and Ymodem downloaders write firmware
into a fixed FAL `download` partition, validate/track total size, write at an offset as chunks arrive, and restart after
download. That is closer to Marginalia's current firmware staging rule than to arbitrary file transfer.

The takeaway for Marginalia is:

- keep OTA destination-specific;
- keep declared size, current offset, and integrity checks in the firmware;
- stage and validate before flashing;
- treat faster transports as replaceable pipes under the same application-level protocol.

Still intentionally out of scope:

- arbitrary SD-card browsing or writes over BLE
- permanent background BLE advertising
- exposing the SD card as a mounted filesystem over BLE

## References

- ESP32-C3 BLE overview: Bluetooth LE 5.0/5.4 certification and ESP-NimBLE support.
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/ble/overview.html
- ESP32-C3 datasheet: BLE PHY support, coded PHY, and LE Data Packet Length Extension.
  https://documentation.espressif.com/esp32-c3_datasheet_en.html
- ESP-IDF BLE data exchange docs: GATT services, reads, writes, notifications, and indications.
  https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c3/api-guides/ble/get-started/ble-data-exchange.html
- NimBLE-Arduino docs via Context7: custom GATT services, writable characteristics, encrypted properties, notifications.
  https://github.com/h2zero/NimBLE-Arduino
- ESP-IoT-Solution BLE L2CAP CoC docs: ESP32-C3 support, stream-oriented flow control, configurable MTU, examples.
  https://docs.espressif.com/projects/esp-iot-solution/en/latest/bluetooth/ble_l2cap_coc.html
- ESP-IDF NimBLE L2CAP CoC examples via Context7: central/peripheral channel setup and 512-byte send examples.
  https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/ble_l2cap_coc
- ESP-IDF BLE 5 throughput example via Context7: 2M PHY with MTU 517 reporting about 175 KB/s.
  https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/ble_50/ble50_throughput
- ESP-IDF Wi-Fi provisioning docs: BLE and SoftAP transports, secure protocomm sessions, and custom endpoints.
  https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32c3/api-reference/provisioning/wifi_provisioning.html
- ESP32-C3 datasheet: Wi-Fi up to 150 Mbps, BLE 2 Mbps PHY, and Wi-Fi/Bluetooth coexistence.
  https://documentation.espressif.com/esp32-c3_datasheet_en.html
- RT-Thread OTA Downloader: HTTP/HTTPS and Ymodem OTA into a fixed FAL download partition.
  https://github.com/RT-Thread-packages/ota_downloader

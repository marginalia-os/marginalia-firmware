# USB SD Card Access

## Idea

Expose the Xteink SD card to a host computer over USB so developers and users can inspect files, pull crash reports,
and drop books or package archives without removing the card or starting the Wi-Fi file server.

## Why It Matters

The firmware already writes panic details to `/crash_report.txt` on the SD card after a crash. Today that file is easy
to mention in the UI but not always easy to retrieve: the device does not currently mount the SD card as a USB volume on
macOS, and serial access only shows live logs.

USB SD access would also improve common workflows:

- copy EPUB files directly from a computer
- retrieve `crash_report.txt` after a panic
- inspect `/.marginalia/packages`, `/.marginalia/inbox`, and `/.marginalia/package-state`
- side-load `.mpkg.zip` archives without relying on Wi-Fi

## Possible Shapes

### USB MSC Mode

Expose the SD card as a USB Mass Storage Class device. This is the most familiar user experience because the card would
appear as a normal removable drive.

This is not viable on the current Xteink ESP32-C3 hardware. Espressif documents the ESP32-C3 USB Serial/JTAG controller
as a fixed-function hardware device for CDC serial, flashing, and JTAG only. It cannot be reconfigured into another USB
class such as Mass Storage Class. Espressif's TinyUSB MSC path is for chips with a real USB device or USB-OTG peripheral,
such as ESP32-S3.

Even on hardware that supports MSC, the firmware and host computer must not write the same filesystem at the same time.
That design would need a dedicated mode that stops reader/package activity, flushes and detaches firmware SD access,
hands the block device to USB, and then remounts after host eject.

### Read-Only Diagnostic Export

Expose a smaller USB endpoint or serial command that can read selected files, starting with `/crash_report.txt`.

This is less convenient for general file transfer but safer because the firmware can keep filesystem ownership and only
stream known files out.

### Custom Serial File Protocol

Use the existing USB CDC serial path for framed file commands instead of trying to mount the SD card. The firmware
already accepts a serial command for screenshots, so this would extend the same debug channel with an explicit protocol:

- `HELLO` / capability negotiation
- `GET /crash_report.txt`
- `LIST /.marginalia/sideload`
- `PUT /.marginalia/sideload/<name>.mpkg.zip <size> <sha256>`
- fixed-size chunks with sequence numbers and checksums
- final hash verification before exposing the file to package install code

This is the best USB path for the current board. It is not as familiar as Finder/Explorer drag-and-drop, but it works
with the available USB hardware and avoids host/firmware filesystem ownership conflicts.

### BLE File Transfer

Bluetooth LE is a better phone-first transfer path than USB serial and should be investigated separately from USB. See
[`bluetooth-file-transfer.md`](bluetooth-file-transfer.md).

### Improve Wi-Fi File Manager First

Keep USB simple and add explicit crash-report download plus package/book shortcuts to the existing web server.

This avoids USB filesystem ownership problems, but still depends on Wi-Fi setup and does not help when networking is
unavailable.

## Initial Recommendation

Do not pursue USB MSC for Xteink/ESP32-C3. Implement a custom serial file protocol only for developer and recovery
workflows, starting with read-only crash-report export. Add write support later only for tightly scoped paths such as
`/.marginalia/sideload/` and use the normal package archive installer after transfer.

For users, prefer SD-card package archive install and a BLE transfer surface. They map better to how people will install
Hub packages from a phone and do not require USB driver tooling.

## Open Questions

- What host-side tool should speak the serial protocol: a small CLI, Hub web app via Web Serial, or both?
- Should serial write support be disabled in release builds until package signature verification exists?
- How should the device UI expose serial transfer mode so auto-sleep does not disconnect mid-transfer?
- Should `PUT` be limited to `/.marginalia/sideload/` and `/Books/`?
- How should interrupted uploads be cleaned up: `.part` suffix, staging directory, or both?

## References

- Espressif ESP32-C3 USB Serial/JTAG documentation: fixed-function CDC/JTAG, not reconfigurable to MSC.
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/usb-serial-jtag-console.html
- Espressif USB Device Stack documentation: TinyUSB MSC is available on USB-device-capable chips such as ESP32-S3.
  https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html
- RT-Thread USB Device documentation: MSC depends on a board USB device peripheral and explicit USB device framework
  support, not only filesystem code.
  https://www.rt-thread.io/document/site/rtthread-studio/drivers/usb-device/rtthread-studio-usb-device/
- RT-Thread DFS documentation: storage is exposed through a filesystem/block-device abstraction, reinforcing the need
  for exclusive block-device ownership when another host writes the medium.
  https://www.rt-thread.io/document/site/programming-manual/filesystem/filesystem/

## Deferred Because

USB MSC is blocked on current hardware. USB serial transfer is still useful, but SD-card package install and BLE transfer
should come first because they solve the common user path without requiring a computer-side serial tool.

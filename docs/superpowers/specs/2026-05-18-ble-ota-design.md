# BLE OTA Design

## Goal

Add BLE firmware update support without widening the file-transfer security surface.

The host can upload a firmware image over the existing authenticated BLE transfer protocol. Firmware stages the image at
a fixed SD-card OTA path, verifies the full byte stream, asks for physical confirmation on the device, then flashes the
validated image into the inactive OTA app partition using the existing firmware flasher.

This PR does not add arbitrary SD browsing, arbitrary path writes, background BLE advertising, a browser companion, or
direct BLE writes into app flash.

## RT-Thread Precedent

RT-Thread's `ota_downloader` keeps OTA destination-specific. The HTTP downloader writes shards into a fixed FAL
`download` partition at the current byte offset and advances that offset as data arrives. The YMODEM downloader parses
the announced file size, checks it against the destination partition, erases the destination, writes chunks at the
current offset, and restarts after the staged image is ready.

Marginalia should keep the same shape but map the destination to existing firmware architecture:

- the BLE protocol gets an explicit `firmware` upload kind;
- the destination is a fixed SD-card staging path, not a caller-provided path;
- the inactive app partition is touched only after the staged file passes size, SHA-256, and ESP image validation;
- the existing `FirmwareFlasher` and `OtaBootSwitch` code owns the final flash and boot-slot switch.

## Protocol

Add a `start_put` kind:

```json
{"op":"start_put","kind":"firmware","name":"firmware.bin","size":6197039,"sha256":"...","resume":true}
```

Rules:

- authentication is required, using the same code or trusted-host flow as other uploads;
- BLE still advertises only while the user is on **File Transfer > Bluetooth Transfer**;
- `name` is accepted for host logging but does not select a destination path;
- firmware stages to `/.marginalia/ota/firmware.bin.part`;
- successful commit replaces `/.marginalia/ota/firmware.bin`;
- `size` must fit in the inactive OTA app partition returned by `esp_ota_get_next_update_partition`;
- `sha256` is required;
- resumable upload uses the existing `.part` file, current byte count, current SHA-256 state, and data-frame sequence
  handling;
- firmware rejects malformed images before any app partition erase or write.

## Firmware Flow

1. `start_put kind=firmware` resolves the inactive OTA partition and validates the declared size.
2. Firmware creates the fixed OTA staging directory if needed.
3. If `resume` is true, firmware hashes the existing `.part` file and reports the resume offset.
4. Data frames append to the `.part` file with the existing upload ACK/window behavior.
5. `commit` verifies final byte count and host-provided SHA-256.
6. Firmware renames the `.part` file to the fixed final staging file.
7. Firmware validates the staged ESP image with `firmware_flash::validateImageFile`.
8. Firmware shows a device-side confirmation prompt before flashing.
9. On confirmation, firmware calls `firmware_flash::flashFromSdPath(..., alreadyValidated=true)`.
10. On successful flash and `otadata` switch, firmware reports restart status and reboots.

If the user rejects the confirmation prompt, firmware leaves the verified staging file in place and reports an error
state. A later upload replaces it through the same fixed path.

## Status

Status JSON should expose progress without revealing secrets:

- `receiving`: includes `received`, `size`, `ack_bytes`, and `resumable`;
- `verifying`: includes `received` and `size`;
- `confirming`: indicates that the host is waiting for physical approval;
- `updating`: includes flash `written` and `size`;
- `restarting`: indicates that flashing succeeded and the device is about to reboot;
- `error`: includes the existing error string.

The host treats `restarting` as success because the BLE connection may drop during reboot.

## Host CLI

Add:

```sh
python3 scripts/ble_transfer.py put-firmware path/to/firmware.bin --code 123456
```

Host behavior:

- compute firmware size and SHA-256 before `start_put`;
- pass `kind=firmware`;
- keep the existing upload resume behavior through `--resume`;
- display `receiving`, `verifying`, `confirming`, `updating`, and `restarting` progress;
- treat final `restarting` status as success after the reported flash byte count matches the staged image size.

The CLI does not expose a generic remote path option.

## Error Handling

Firmware fails before touching app flash when:

- authentication fails;
- declared size is below the minimum ESP image size;
- declared size exceeds the inactive app partition;
- the staging file cannot be opened, written, renamed, or read;
- byte count or SHA-256 does not match;
- ESP image validation fails;
- the user rejects the device confirmation prompt.

Firmware may fail during flash when erase, write, or `otadata` switching fails. In that case, the currently running app
remains the active boot target unless `otadata` switching has already succeeded. This matches the existing flasher
failure model.

## Rollback Scope

This first BLE OTA PR preserves the previous app slot and uses the existing `OtaBootSwitch` path. It does not change
bootloader rollback policy or add a new post-boot self-test flow. That work should be separate because it affects
bootloader configuration and app-validity decisions beyond BLE transfer.

## Validation

Run:

```sh
python3 -m py_compile scripts/ble_transfer.py
git diff --check
./bin/clang-format-fix
pio run
```

Hardware validation:

1. Flash the current branch over USB.
2. Open **File Transfer > Bluetooth Transfer** on the device.
3. Upload a firmware image with `put-firmware`.
4. Interrupt and resume at least one upload using `--resume`.
5. Confirm on device.
6. Verify flash progress reaches the firmware size, the device restarts, and the new version boots.


# BLE Diagnostic Download Resume Design

## Goal

Allow interrupted BLE diagnostic downloads to resume without widening the firmware read surface. This applies only to the
existing authenticated `start_get` kinds:

- `crash_report`, backed by `/crash_report.txt`
- `package_state`, backed by `/.marginalia/package-state/<safe-package-id>.json`

This does not add arbitrary path reads, file browsing, background BLE advertising, or OTA behavior.

## RT-Thread Precedent

RT-Thread's OTA downloader keeps transfers destination-specific and offset-driven. Its HTTP path discovers total file
size, erases a fixed `download` FAL partition, writes each shard at `begin_offset`, increments that offset by the shard
length, and requests subsequent shards by current position. Its YMODEM path similarly parses total size, checks it
against the destination partition, writes chunks at `update_file_cur_size`, and increments that current size.

For Marginalia diagnostic downloads, the direction is reversed: firmware reads from an allowlisted file and the host
stores the partial output. The same reliability rule applies: resume from a validated current byte offset and keep total
size visible in status.

## Protocol

`start_get` accepts optional fields:

```json
{"op":"start_get","kind":"crash_report","offset":4096,"chunk_size":160}
```

```json
{"op":"start_get","kind":"package_state","package_id":"org.example.package","offset":4096,"chunk_size":160}
```

Rules:

- `offset` defaults to `0`.
- `chunk_size` defaults to the existing download chunk size.
- `chunk_size` is bounded by the firmware's existing small BLE frame buffer.
- firmware validates `offset <= file_size`.
- firmware requires resumed offsets to align to `chunk_size`, because `data-out` sequence numbers are chunk indexes.
- firmware seeks the allowlisted file to `offset`.
- firmware sets `sent = offset` and `downloadSequence = offset / chunk_size`.
- status continues to include total `size` and current `sent`.

## Host Behavior

The CLI writes to `<output>.part`. With `--resume`, it uses the partial file size as `offset`, appends to the partial
file, and starts the expected sequence at `offset / chunk_size`. Without `--resume`, any stale partial is removed and the
download starts at zero.

The final output path is replaced only after firmware reports `sent` and the host byte count equals the reported total
file size.

## Follow-Up

BLE OTA should reuse the same ideas but remain a separate PR: explicit destination, offset/size progress, integrity
checks, and a staging area that cannot be confused with diagnostic downloads.

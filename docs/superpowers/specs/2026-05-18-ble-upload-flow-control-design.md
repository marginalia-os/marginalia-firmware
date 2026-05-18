# BLE Upload Flow Control Design

## Problem

BLE uploads currently default to a windowed write-without-response mode, but the receiver progress interval is fixed.
During resumable EPUB validation, the default path could timeout before the host saw enough progress from the device.
Switching every resumable upload to write-with-response would improve reliability, but it would also sidestep the
high-throughput path needed for larger future transfers.

## Approach

Keep write-without-response as the default upload strategy, but make it explicitly flow-controlled:

- The host includes an `ack_bytes` value in `start_put` when it uses windowed mode.
- Firmware validates `ack_bytes` and publishes progress whenever it receives at least that many new bytes, or when the
  upload completes.
- The host sends one window of chunks, waits until firmware reports that byte offset, then continues.
- If a window times out, the host fails with the last device error or the missing byte offset. The transfer can then be
  retried with `--resume`.

The raw `no-response` mode remains available for explicit speed experiments. `response` mode remains available for
debugging and conservative transfers, but it is not the default.

## Data Flow

1. Host authenticates with a trusted host or six-digit code.
2. Host sends `start_put` with `resume`, `chunk_size`, and `ack_bytes`.
3. Firmware opens or resumes the `.part` file and returns `state: "receiving"` with the current `received` offset.
4. Host streams chunks up to the next ACK target.
5. Firmware writes chunks in order, updates SHA-256 state, and emits status when the negotiated ACK interval is reached.
6. Host waits for `received >= target` before sending the next window.
7. Host sends `commit`; firmware verifies size and SHA-256 before moving or installing the file.

## Error Handling

Firmware rejects zero or unreasonable `ack_bytes` values. Host-side timeouts are treated as failed transfers, not silent
fallbacks, because the user may want to resume from the last confirmed offset. Existing partial cleanup behavior remains:
non-resumable interrupted uploads clean the `.part` file, while resumable interrupted uploads keep it.

## Testing

Local checks should cover Python syntax, transfer help output, whitespace, formatting, build, and static analysis. Hardware
validation should retry the EPUB case that previously needed `--transfer-mode response`, first with default windowed mode
and then with an interrupted `--resume` run.

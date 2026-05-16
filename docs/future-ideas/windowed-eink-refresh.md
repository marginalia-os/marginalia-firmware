# Windowed E-Ink Refresh

## Idea

Investigate whether reader and system UI updates can use rectangular e-ink refreshes for small regions such as status
bars, selection rows, progress indicators, and package install screens.

## Current State

The lower `EInkDisplay` driver already exposes an experimental `displayWindow(x, y, w, h)` path. It configures the
controller RAM window, writes only the selected region, and performs a fast refresh. The higher firmware layers do not
currently expose this through `HalDisplay` or `GfxRenderer`; `GfxRenderer::displayWindow()` is only a commented stub.

Reader page turns still change most of the screen, especially in inverted dark mode. Windowed refresh is therefore not
the primary fix for full-page text ghosting. The current dark-mode path should continue using package-owned reader
cleanup cadence for full reader pages.

## Risks

- Window X and width must be byte-aligned.
- Orientation transforms need to map logical rectangles to panel rectangles.
- X3 and X4 may need different RAM baseline handling.
- Dirty-region tracking could add complexity to every activity if it is not carefully scoped.

## Suggested First Experiment

Expose a low-level `HalDisplay::displayWindow()` wrapper and a matching `GfxRenderer::displayWindow()` method, then test
only one narrow caller: status bar or focused-row updates. Do not route reader page turns through this until window
baseline handling is verified on real hardware.

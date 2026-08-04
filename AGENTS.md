# AGENTS.md

PlatformIO **library** for a battery coulometer on an ESP8266 (Wemos D1 Mini) using an INA226 current sensor. `library.json` makes it consumable as a PlatformIO dependency; the root `platformio.ini` exists only for editor tooling.

## Metadata

- This file (`AGENTS.md`) is **not committed/published to GitHub** — it is internal agent guidance only. Treat `library.json` and `README.md` as the visible sources of truth for version/license/author/repo.
- Version `1.0.0`, GPL-3.0, author Olivier FOURNET (olivier.fournet@free.fr). Repo: https://github.com/Fo170/CoulombAh (branch `main`). Keep these in sync in `library.json` and `README.md`.

## Build

- Toolchain: PlatformIO (`pio`), target `d1_mini` (espressif8266 / Arduino). Serial monitor baud: `115200`.
- `.pio/` is build output and gitignored. Not currently a git repo.

## Critical gotcha

- `src/` contains **only** header libraries — there is **no `src/main.cpp`**, so `pio run` at the root fails as-is.
- Real firmware examples are self-contained PlatformIO projects under `exemples/` (note French spelling, not `examples`):
  - `exemples/Coulomb/` — usage of `Coulomb.h`
  - `exemples/AmpereHeure/` — usage of `AmpereHeure.h`
  - Build them with `pio run -d exemples/Coulomb` / `pio run -d exemples/AmpereHeure`. Each references the repo root via `lib_extra_dirs = ../..`.
- The headers are resolved as a library (repo root), so editing example code + headers is the normal workflow.

## Architecture

- `src/Coulomb.h` and `src/AmpereHeure.h` are **header-only single-class libraries** (no `.cpp`). Logic is inline in the headers.
- `Coulomb`: integrates current × Δt (micros()-based) into Coulombs (1 C = 1 A·s). `AmpereHeure`: wraps the same math (1 Ah = 3600 C) plus battery SoC, charge/discharge efficiency, low-battery threshold, runtime estimates.
- Both support optional EEPROM persistence (4-byte `float` per counter). When constructing with an EEPROM address, the persisted value is loaded in the constructor and must be saved explicitly via `save()` (e.g. periodically); the destructor also saves.
- Timing: `addMeasurement(current, microsTimestamp)` guards against `micros()` overflow (~70 min); intervals are validated (`0 < interval ≤ 3600s`), invalid intervals are dropped.
- Exponential filtering: default `_filterAlpha = 0.99`; `filterAlpha` closer to 1 = stronger smoothing. First measurement only seeds state.
- INA226 I2C config in the example: address `0x40`, shunt `0.01 Ω`, max current `4.0 A`. (The current example firmware simulates the current reading; no INA226 driver is bundled.)

## Conventions

- Code comments, doc headers, and Serial output are written in **French**; keep that style when editing.
- Battery parameters are configured via `#define`s in `exemples/*/src/main.cpp` (e.g. `BATTERY_CAPACITY_AH`, EEPROM addresses, intervals).

## Testing

- `test/` has no actual tests; do not expect `pio test` to pass.

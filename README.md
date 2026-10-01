<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->

# felitronics-appkit

App-side **JUCE** infrastructure shared by the Darwin's Cat product family (OrbitCab, TabbyEQ,
OrbitCapture). Deliberately a separate repo from [felitronics-core](https://github.com/darwinscat/felitronics-core):
core is pure, JUCE-free, real-time-safe DSP — everything here assumes a JUCE app or plugin around it.

Header-only. The CMake target adds an include path and nothing else — **the consumer supplies JUCE**.

## What's inside

| Header | Needs | What |
|---|---|---|
| `felitronics/appkit/UpdateCompare.h` | nothing (constexpr, JUCE-free) | The update-badge rule: numeric semver for clean release builds; a `git describe` dev stamp counts older than any release; hostile tags reject safely. |
| `felitronics/appkit/UpdateChecker.h` | `juce_events`, `juce_data_structures` | The opt-in GitHub-release update check: user-click only (never on launch), owned worker thread joined on destruction, silent failure, badge persisted in the product's `PropertiesFile`. |
| `felitronics/appkit/Brand.h` | `juce_gui_basics` | The Darwin's Cat identity, consolidated from the diverged orbitcab/orbit-capture copies: palette (`brand::violet/lilac/orange`), the orbit "target" mark (`drawOrbit`), the fixed 8-slot palette, the large-glyph `GearButton`. |
| `felitronics/appkit/TextPrompt.h` | `juce_gui_basics` | One-line modal text prompt (OK/Enter · Cancel/Esc), brand-styled. |
| `felitronics/appkit/DeviceSpec.h` | `juce_core` | The parsed "device" spec model — tube/BJT/FET/IC/DSP/diode/transformer/tape with counts, hybrids like `"tube:1,pnp:1"`; malformed input drops entries, never garbage. Pure data, unit-tested here. |
| `felitronics/appkit/DeviceGlyph.h` | `juce_gui_basics` | Schematic device glyphs (triode, PNP, JFET, op-amp, chip, diode, transformer, tape reel) + the glowing `DeviceStrip` row, per-family stroke/glow colours; the four transfer-shape marks (`drawShapeGlyph`, `ShapeGlyph::tanh/atan/cubic/asym`); any catalogue glyph by key (`drawGlyph (g, r, "shape-atan", c)`). Drawn from `DeviceGlyphPaths.h`. |
| `felitronics/appkit/DeviceGlyphPaths.h` | nothing (JUCE-free, generated) | The glyph geometry as SVG path strings (`glyphpaths::all`, `glyphpaths::find (key)`) — the same data as `assets/glyphs/*.svg`. |
| `felitronics/appkit/Flicker.h` | `juce_core` | The shared one-pole "heater glow" shimmer kernel: two detuned sines + jitter, one-pole smoothed. Drives `DeviceStrip` (and OrbitCab's power-tube heaters) so the whole family flickers the same way. |
| `felitronics/appkit/LevelMeter.h` | `juce_audio_basics`, `juce_gui_basics` | Thin vertical dBFS peak meter (from OrbitCab): instant-attack/smooth-release ballistics + peak-hold, zoomable range (`setRange`), scale ticks/labels. Fed on the message thread — a GUI timer (~30 Hz) reads the processor's atomic per-block peak and calls `setLevel`. Calibrator extras: `setGreenZone`/`zone` target corridor, `setClipCeiling` warn-band, `setRefLines` fixed grid, clickable clip lamp (`setClipLatched`/`onClipClick`). |
| `felitronics/appkit/LevelHistory.h` | `juce_audio_basics`, `juce_gui_basics` | Scrolling peak-history strip (dBFS), generalized from OrbitCapture's MicHistory. `push` scrolls one column per GUI tick; visible window = `capacityTicks` / feed rate. Two overlays tint the trace: `setRefLines` (fixed dashed calibration grid, the primary surface) or `setGreenZone` (moving corridor); `setNoiseFloor` draws a room-quiet line, `setCurrentDb` a live corner readout, `setClipCeiling` full-height red bars for over-ceiling columns. `peakDb` holds ~1.5 s then decays. |
| `felitronics/appkit/IrWaveView.h` | `juce_audio_utils` | An impulse response, drawn, with TRIM (drag to truncate) and the HPF/LPF response curve with a draggable corner point per enabled filter drawn on it — moved from OrbitCab's WaveformDisplay for the family's cabinet blocks. Decodes on the message thread; reports through `onTrimChanged` / `onHpfChanged` / `onLpfChanged`; the owner pushes the parameters back with `setFilters` / `setTrimFraction`, and draws its own analyser behind the impulse through `paintSpectrumUnder`. `irwave::` holds the plain-number geometry and envelope math. |
| `felitronics/appkit/CallOut.h` | `juce_gui_basics` | `launchCallOut`: a CallOutBox parented to the editor, not the desktop — a desktop call-out orphans on screen when the plugin window closes. |
| `felitronics/appkit/VersionBadge.h` | `juce_gui_basics` | The clickable "vX.Y.Z / format" corner badge + update popover (brand mark, full build stamp with GitHub links, opt-in "Check for updates"). Fronts the product's `UpdateChecker` adapter; identity/build-stamp/dependency-line in its `Config`. |
| `felitronics/appkit/VersionStamp.h` | `juce_gui_basics` | The build stamp for a bottom bar — "v0.6.0 · Standalone", the update dot, and a click that opens whatever you point it at. A build that only descends from a release keeps a "+". |
| `felitronics/appkit/PerfBadge.h` | `juce_gui_basics` | The clickable "latency · CPU%" badge + live per-stage DSP-load popover; the product's stage rows (label + colour) are `Config` data, stats pushed as snapshots. |
| `felitronics/appkit/WebpImage.h` | `juce_graphics` + `felitronics::appkit_webp` (CMake `FELITRONICS_APPKIT_WEBP=ON`, fetches libwebp) | `juce::Image` ↔ WebP for the one picture a device pack ships: lossy colour at a chosen quality, the alpha plane lossless, premultiply/unpremultiply handled — a cut-out's edge survives the trip. |

The brand itself — the Darwin's Cat mark and the Michroma wordmark face — is **compiled in**
(`felitronics/appkit/BrandAssets.h`: `brand::drawCat`, `brand::wordmarkTypeface`), so a window opened
by any consumer wears it without that consumer doing anything. The embedded face is a subset (ASCII
plus a handful of marks); see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the OFL terms and
the command that regenerates it.

The full originals still live in [`assets/`](assets/) for a product that wants the whole face in its
own binary: `juce_add_binary_data(MyAssets SOURCES ${felitronics_appkit_SOURCE_DIR}/assets/Michroma-Regular.ttf …)`
(`felitronics_appkit_SOURCE_DIR` is set by `FetchContent_MakeAvailable`).

### Glyphs outside JUCE

The device and shape glyphs are one geometry with two readers. `tools/gen-glyphs.mjs` (Node, no
dependencies: `node tools/gen-glyphs.mjs`) writes both `assets/glyphs/<key>.svg` and
`include/felitronics/appkit/DeviceGlyphPaths.h`; `appkit_glyph_assets_tests` fails if they disagree,
and CI re-runs the generator and fails if the committed outputs differ from what it writes. Change a
glyph in the generator, never in either output, and commit what it writes.

Keys: `tube` `bjt` `fet` `ic` `dsp` `diode` `transformer` `tape` (one per `DeviceType`) and
`shape-tanh` `shape-atan` `shape-cubic` `shape-asym`.

A non-JUCE consumer (the website draws them on a canvas) vendors the SVG files as they are and
draws each `<path d>` on its own — the file holds geometry only, no colour and no stroke width:

```js
// frame: viewBox 0 0 100 100 mapped onto a square of side 2R centred in the cell
ctx.save();
ctx.translate(cx - R, cy - R);
ctx.scale(R / 50, R / 50);
ctx.lineWidth = Math.max(1, 0.11 * R) * 50 / R;   // max(1 px, 0.11 R), in frame units
ctx.lineCap = ctx.lineJoin = 'round';
ctx.strokeStyle = colour;
for (const d of paths) ctx.stroke(new Path2D(d));  // each path separately, in file order
ctx.restore();
```

Leads reach up to 8 % past the 100 × 100 box, and a round cap adds half the stroke (2.75 units)
beyond the tip, so ink lands up to 10.75 % of the frame's side outside it. Inset the cell by 12 % of
its side on every edge before mapping the frame onto it (or do not clip): that is what the built-in
rows and the tests use, it holds the caps with about 4 % of the cell to spare, and the 8 % the leads
alone suggest lets the caps out by about 1 % of the cell. Nothing is filled. Stroke the paths one at
a time: a closed ring and an open lead that crosses it are kept in separate paths because one stroked
path is filled non-zero and the crossing would cancel.

The files are AGPL-3.0-or-later like the rest of this repo; each carries the
`SPDX-License-Identifier: AGPL-3.0-or-later` line and the copyright line in a leading comment — keep
both when vendoring. An SVG here is generated output, not source: a third party that ships the files
ships the licence text (`LICENSE`) alongside them, and their Corresponding Source is the generator,
`tools/gen-glyphs.mjs` — point to it at the tag the files came from.

Consumers subclass `UpdateChecker` as a thin adapter that bakes in their `Config` (repo slug,
product name, version string, settings accessor, legacy settings keys).

## Consume

```cmake
include(FetchContent)
FetchContent_Declare(felitronics_appkit
    GIT_REPOSITORY https://github.com/darwinscat/felitronics-appkit.git
    GIT_TAG        vX.Y.Z)
FetchContent_MakeAvailable(felitronics_appkit)
target_link_libraries(app PRIVATE felitronics::appkit)  # + your juce_events / juce_data_structures
```

## Build & test

```bash
cmake -B build -DFELITRONICS_APPKIT_TESTS_WITH_JUCE=ON   # ON fetches JUCE for the compile gate
cmake --build build -j && ctest --test-dir build
```

The JUCE-free tier (`UpdateCompare`, the glyph assets) always tests offline. The JUCE tier runs the `DeviceSpec`
parsing unit and compiles every JUCE header under `juce_recommended_warning_flags` + `-Werror` —
the exact flag class the products build with — then smoke-runs the gate without touching the
network (pass `--live` to the binary manually for one real end-to-end GitHub check).

## License

AGPL-3.0-or-later — see [LICENSE](LICENSE).

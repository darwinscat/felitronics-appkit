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
| `felitronics/appkit/InputPick.h` | nothing (JUCE-free) | Which inputs of an audio interface play, for a standalone (moved from the cab builders 0.5.0): `InputChoice` one input, two summed at half each, or a pair, kept as text `"4"`, `"3+4"`, `"3/4"`, falling back to IN 1, or to the first inputs open (`fittedTo`); `InputPick` takes every input of the device and writes the choice out (one or a sum to every output, a pair to the first two), every change crossfaded within 20 ms from wherever the sound stands (each input's gain slewed), the peak of every input kept for its meter (`takeLevel`), the device's partly open inputs mapped (`prepare`); no allocation or lock on the audio thread. `InputMemory` keeps a device's choice by its name, `inputOf <device>`, through the app's own store (two functions; a `PropertiesFile`, say). |
| `felitronics/appkit/InputList.h` | `juce_gui_basics` | IN 1..IN n with a level meter each, the ones heard lit, an optional word in a lit cell ("L", "R"); a click is the app's to read; eight to a column under the device's name. The look comes from the app (`InputListStyle`: colours, font, text drawing). `drawInputMeter` alone for a box elsewhere; `openInputBits` turns a device's active inputs into `InputPick`'s bits. |
| `felitronics/appkit/LicenceKey.h` | `felitronics::appkit_licence` (Monocypher's Ed25519, compiled only for a product that links it) | A paid product's key: the fields (product, name, e-mail, number, purchase day, last release day covered, kind) signed with Ed25519 and written as one base32 block; `read` checks it offline against the product's public key (valid / another product's / a newer format / damaged), `covers` says whether this release is covered, `status` says trial, demo, licensed or updates over. JUCE-free. The plain types and the day arithmetic are in `LicenceTypes.h`, which needs neither JUCE nor Monocypher (the screens include that). The bytes and test vectors: [`docs/licence-key.md`](docs/licence-key.md). |
| `felitronics/appkit/LicenceKeyIssue.h` | `felitronics::appkit_licence` | Making keys (tests, the issuing tool): `publicKeyOf (seed)`, `sign`, `issue`. A product never includes it. |
| `felitronics/appkit/LoopPlayer.h` | `juce_audio_formats` | A guitar loop played in place of a product's input, so it can be heard with no instrument plugged in (lifted from OrbitAmp). `find` lists the .wav / .aif / .aiff / .flac in the folders the product names, in natural order; `load` decodes on the message thread, the whole file or nothing; `fill` on the audio thread replaces the input (every channel, a mono loop on all) under a try-lock, another rate stepped through by linear interpolation, no allocation; a stop rewinds. |
| `felitronics/appkit/LevelHistory.h` | `juce_audio_basics`, `juce_gui_basics` | Scrolling peak-history strip (dBFS), generalized from OrbitCapture's MicHistory. `push` scrolls one column per GUI tick; visible window = `capacityTicks` / feed rate. Two overlays tint the trace: `setRefLines` (fixed dashed calibration grid, the primary surface) or `setGreenZone` (moving corridor); `setNoiseFloor` draws a room-quiet line, `setCurrentDb` a live corner readout, `setClipCeiling` full-height red bars for over-ceiling columns. `peakDb` holds ~1.5 s then decays. |
| `felitronics/appkit/IrWaveView.h` | `juce_audio_utils` | An impulse response, drawn, with TRIM (drag to truncate) and the HPF/LPF response curve with a draggable corner point per enabled filter drawn on it — moved from OrbitCab's WaveformDisplay for the family's cabinet blocks. Decodes on the message thread; reports through `onTrimChanged` / `onHpfChanged` / `onLpfChanged`; the owner pushes the parameters back with `setFilters` / `setTrimFraction`, and draws its own analyser behind the impulse through `paintSpectrumUnder`. `irwave::` holds the plain-number geometry and envelope math. |
| `felitronics/appkit/CallOut.h` | `juce_gui_basics` | `launchCallOut`: a CallOutBox parented to the editor, not the desktop — a desktop call-out orphans on screen when the plugin window closes. |
| `felitronics/appkit/VersionBadge.h` | `juce_gui_basics` | The clickable "vX.Y.Z / format" corner badge + update popover (brand mark, full build stamp with GitHub links, opt-in "Check for updates"). Fronts the product's `UpdateChecker` adapter; identity/build-stamp/dependency-line in its `Config`. Speaks the system's language (`I18n.h`). A paid product sets `Config::purchase`: the licence block replaces "Feed the Cat", "Enter key" / "Ask for a free licence" open as pages of the same window (`showAbout (over, Page::enterKey)` opens straight on one), and the badge opens the About dialog rather than a call-out (a call-out closes when the app loses focus, taking a pasted key with it). Without it the window is the free one, unchanged. |
| `felitronics/appkit/I18n.h` | `juce_core` | The words appkit's windows say, in the twelve languages of darwinscat.com (en de uk ru cs es fr it pl pt ro tr): `tr ("English sentence")`, `trCount` with CLDR's plural rules for whole numbers, `fill` for `{placeholders}`. The system's display language, English when it is not one of the twelve; `setLanguage` overrides. Never touches `juce::LocalisedStrings`. Tables: `i18n/<lang>.h`, keyed by the English sentences of `i18n/en.h`, UTF-8 with a byte order mark (MSVC reads them right without `/utf-8`). |
| `felitronics/appkit/LicencePages.h` | `juce_gui_basics` | The paying half of a paid product's About window: `licence::Purchase` (the product's standing, its key function, the shop / renewal URLs, the free-licence address), the licence block, the "Enter key" page (paste or drop the .txt, the verdict in words) and the free-licence page, whose button opens the mail app with a percent-encoded `mailto:` letter of at most 1800 characters (the address stays on the page, with a Copy button, for when no mail app opens) — nothing is sent from the product. |
| `felitronics/appkit/LicenceNotice.h` | `juce_gui_basics` | The after-trial card a paid product lays over its own window: the product keeps playing, "Buy · price", "Enter key", the four free licences, "Ask for a free licence", and "Not now", which counts 5 4 3 2 1 each time the card comes back on screen (also when only a parent of it was hidden). For a key whose updates ended, its own sentence and "Renew at 50 % off". |
| `felitronics/appkit/LicenceStore.h` | `juce_core` | Where a paid product keeps its key and its first day: `licence-key.txt` (the accepted key alone, as issued — never the letter) and `first-run.txt` (`YYYY-MM-DD`, local date, created by the first instance to start; a date before 2026 reads as 2026-01-01) in the folder `SettingsStore` uses, so plug-in and standalone share them; written through a temporary file that is read back before it takes the name. `today()` is the local calendar day. |
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

The JUCE tier fetches JUCE `8.0.17-security` by default. The headers build on JUCE 9 as well:
`-DFELITRONICS_APPKIT_JUCE_TAG=9.0.5` runs the same tier against it (CI runs both), and
`-DFELITRONICS_APPKIT_JUCE_SRC=/path/to/JUCE` uses a local checkout instead of fetching.

The JUCE-free tier (`UpdateCompare`, the glyph assets) always tests offline. The JUCE tier runs the `DeviceSpec`
parsing unit and compiles every JUCE header under `juce_recommended_warning_flags` + `-Werror` —
the exact flag class the products build with — then smoke-runs the gate without touching the
network (pass `--live` to the binary manually for one real end-to-end GitHub check).

## License

AGPL-3.0-or-later — see [LICENSE](LICENSE).

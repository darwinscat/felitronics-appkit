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
| `felitronics/appkit/LevelMeter.h` | `juce_audio_basics`, `juce_gui_basics` | Thin vertical dBFS peak meter (from OrbitCab): instant-attack/smooth-release ballistics + peak-hold, zoomable range (`setRange`), scale ticks/labels. Fed on the message thread — a GUI timer (~30 Hz) reads the processor's atomic per-block peak and calls `setLevel`. |
| `felitronics/appkit/CallOut.h` | `juce_gui_basics` | `launchCallOut`: a CallOutBox parented to the editor, not the desktop — a desktop call-out orphans on screen when the plugin window closes. |
| `felitronics/appkit/VersionBadge.h` | `juce_gui_basics` | The clickable "vX.Y.Z / format" corner badge + update popover (brand mark, full build stamp with GitHub links, opt-in "Check for updates"). Fronts the product's `UpdateChecker` adapter; identity/build-stamp/dependency-line in its `Config`. |
| `felitronics/appkit/PerfBadge.h` | `juce_gui_basics` | The clickable "latency · CPU%" badge + live per-stage DSP-load popover; the product's stage rows (label + colour) are `Config` data, stats pushed as snapshots. |

Brand *assets* (Michroma font + OFL license, `catlogo.svg`) live in [`assets/`](assets/) — embed them
from your app's CMake: `juce_add_binary_data(MyAssets SOURCES ${felitronics_appkit_SOURCE_DIR}/assets/Michroma-Regular.ttf …)`
(`felitronics_appkit_SOURCE_DIR` is set by `FetchContent_MakeAvailable`).

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

The JUCE-free tier (`UpdateCompare`) always tests offline. The JUCE tier compiles `UpdateChecker.h`
under `juce_recommended_warning_flags` + `-Werror` — the exact flag class the products build with —
and smoke-runs it without touching the network (pass `--live` to the binary manually for one real
end-to-end GitHub check).

## License

AGPL-3.0-or-later — see [LICENSE](LICENSE).

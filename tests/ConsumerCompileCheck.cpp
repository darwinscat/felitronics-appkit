// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// CONSUMER-SIMULATION gate for UpdateChecker.h — the JUCE half of appkit that plain ctest can't
// cover. Compiled against a real JUCE under juce_recommended_warning_flags + -Werror (the exact
// flag class TabbyEQ/OrbitCab build with), then smoke-run: construct with null settings, exercise
// the message-thread getters, destruct (must join a never-started worker without hanging). NO
// network by default — pass --live to also run one real end-to-end check against GitHub (dev-box
// only; CI stays offline and deterministic).
#include <felitronics/appkit/AudioSettingsPanel.h>
#include <felitronics/appkit/Brand.h>
#include <felitronics/appkit/BrandHeader.h>
#include <felitronics/appkit/Download.h>
#include <felitronics/appkit/LevelMeter.h>
#include <felitronics/appkit/IconButton.h>
#include <felitronics/appkit/CallOut.h>
#include <felitronics/appkit/DeviceGlyph.h>
#include <felitronics/appkit/DeviceSpec.h>
#include <felitronics/appkit/Flicker.h>
#include <felitronics/appkit/NotifyPing.h>
#include <felitronics/appkit/PerfBadge.h>
#include <felitronics/appkit/SettingsStore.h>
#include <felitronics/appkit/TextPrompt.h>
#include <felitronics/appkit/UpdateChecker.h>
#include <felitronics/appkit/VersionBadge.h>

#include <cstdio>
#include <cstring>
#include <thread>

struct ChkAdapter : felitronics::appkit::UpdateChecker
{
    ChkAdapter()
        : felitronics::appkit::UpdateChecker ({ .ownerRepo      = "darwinscat/felitronics-appkit",
                                                .productName    = "Appkit Gate",
                                                .currentVersion = "v0.1.0-7-gdeadbee",
                                                .settings       = {} }) {}
};

int main (int argc, char** argv)
{
    // Child mode for the CROSS-PROCESS lock smoke: hammer one key in the given store dir and
    // verify own writes survive. Spawned by the parent below; exits 0 on success.
    if (argc > 2 && std::strcmp (argv[1], "--settings-child") == 0)
    {
        felitronics::appkit::SettingsStore s ("Darwin's Cat", "AppkitGate", juce::File (argv[2]));
        for (int i = 0; i < 100; ++i)
            if (! s.set ("kc", i) || (int) s.get ("kc", -1) != i) return 1;
        return 0;
    }

    const bool live = argc > 1 && std::strcmp (argv[1], "--live") == 0;
    int failures = 0;
    auto ok = [&failures] (bool cond, const char* what)
    {
        if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what); }
    };

    juce::MessageManager::getInstance();   // the AsyncUpdater base wants a live MessageManager

    // Brand.h / TextPrompt.h are GUI headers — this tier only gates that they COMPILE clean and
    // link under the consumer flag set (no window/system usage in a headless CI runner).
    {
        auto* mark = &felitronics::appkit::brand::drawOrbit;       (void) mark;
        auto* rings = &felitronics::appkit::brand::drawOrbitRings; (void) rings;
        auto* width = &felitronics::appkit::brand::textWidth;      (void) width;
        auto* prompt = &felitronics::appkit::textPrompt;           (void) prompt;
        ok (felitronics::appkit::brand::violet != felitronics::appkit::brand::orange,
            "brand palette is distinct");
        ok (felitronics::appkit::brand::wordmarkFont (nullptr, 14.0f).getHeight() > 13.0f,
            "wordmarkFont: bold system fallback when the typeface is null");
    }

    // LevelMeter: a plain Component — safe to construct and feed headless (no peer, so repaint()
    // is inert and paint() never runs). Exercises the ballistics/setRange paths under real JUCE.
    {
        felitronics::appkit::LevelMeter meter;
        meter.setSize (14, 120);
        meter.setRange (-24.0f, 6.0f);
        for (int i = 0; i < 30; ++i) meter.setLevel (i < 3 ? 0.9f : 0.0f);   // attack, then release + hold decay
        ok (meter.getWidth() == 14 && meter.getHeight() == 120, "LevelMeter constructs and accepts levels headless");

        const felitronics::appkit::IconButton icon (felitronics::appkit::IconButton::Kind::settings);
        ok (icon.colour == juce::Colour (0xffc0c0c8) && icon.panelColour == juce::Colour (0xff1b1b1f)
                && ! icon.framed,
            "IconButton defaults pin OrbitCab's neutral/panel look");
    }

    // DeviceGlyph.h / Flicker.h: headless smoke — run the shared shimmer kernel (levels must hold
    // the documented [0.5, 1] band around the 0.84 rest), then software-render the flickering strip
    // and the static popup row into a juce::Image (no window — CI-safe): the glyphs must actually
    // put paint down. DeviceSpec.h behaviour itself is unit-covered by appkit_device_spec_tests.
    {
        using felitronics::appkit::DeviceStrip;
        using felitronics::appkit::DeviceType;
        namespace fk = felitronics::appkit;

        fk::Flicker<fk::kMaxDeviceGlyphs> flick;
        bool band = flick[0] > 0.83f && flick[0] < 0.85f;   // rest level before the first tick
        for (int f = 0; f < 90; ++f)                        // ~3 s at the products' 30 Hz
        {
            flick.tick();
            for (int i = 0; i < flick.size(); ++i)
                band = band && flick[i] >= 0.5f && flick[i] <= 1.0f;
        }
        ok (band, "flicker levels hold the [0.5, 1] band around the 0.84 rest");
        ok (flick[-5] >= 0.5f && flick[999] <= 1.0f, "out-of-range flicker channels clamp, not UB");

        const auto spec = fk::parseDeviceSpec ("tube:1,pnp:1");
        ok (fk::deviceSpecCount (spec) == 2, "hybrid spec parses to 2 glyphs");
        ok (fk::deviceGlow (DeviceType::tube) != fk::deviceGlow (DeviceType::pnp),
            "hybrid glows two family colours");

        auto paintedPixels = [] (const juce::Image& img)
        {
            int n = 0;
            for (int y = 0; y < img.getHeight(); ++y)
                for (int x = 0; x < img.getWidth(); ++x)
                    if (img.getPixelAt (x, y).getAlpha() > 0) ++n;
            return n;
        };

        DeviceStrip strip;
        strip.setSize (120, 28);
        strip.set (spec);
        for (int f = 0; f < 3; ++f) strip.tick();
        juce::Image stripImg (juce::Image::ARGB, 120, 28, true);
        {
            juce::Graphics ig (stripImg);
            strip.paint (ig);
        }
        ok (paintedPixels (stripImg) > 100, "DeviceStrip paints glyphs + glow headlessly");

        juce::Image rowImg (juce::Image::ARGB, 120, 28, true);
        {
            juce::Graphics ig (rowImg);
            fk::drawDeviceSpecStatic (ig, { 0.0f, 0.0f, 120.0f, 28.0f }, spec);
        }
        ok (paintedPixels (rowImg) > 50, "static popup row paints headlessly");
    }

    // SettingsStore: functional smoke in an isolated temp dir (never the developer's real
    // app-data), incl. the cross-process-lock discipline pinned in-process: two threads doing
    // read-modify-write set() on DIFFERENT keys through two store instances (same lock NAME) —
    // without the InterProcessLock the stale-load RMW erases the other thread's key.
    {
        using felitronics::appkit::SettingsStore;
        const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("appkit-gate-" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30)));
        SettingsStore s ("Darwin's Cat", "AppkitGate", tmp);
        ok (! s.file().existsAsFile(), "fresh store: no file yet");
        ok (s.load().getDynamicObject() != nullptr, "missing file loads as an empty object");
        ok (s.set ("alpha", 1), "set() creates the file");
        ok ((int) s.get ("alpha", 0) == 1, "roundtrip");
        s.file().replaceWithText ("{ definitely not json");
        ok (s.load().getDynamicObject() != nullptr, "corrupt file loads as an empty object (tolerant)");
        ok (s.set ("alpha", 2) && (int) s.get ("alpha", 0) == 2, "store recovers after corruption");

        SettingsStore a ("Darwin's Cat", "AppkitGate", tmp), b ("Darwin's Cat", "AppkitGate", tmp);
        bool aOk = true, bOk = true;
        std::thread ta ([&] { for (int i = 0; i < 100; ++i) { aOk = aOk && a.set ("ka", i); aOk = aOk && (int) a.get ("ka", -1) == i; } });
        std::thread tb ([&] { for (int i = 0; i < 100; ++i) { bOk = bOk && b.set ("kb", i); bOk = bOk && (int) b.get ("kb", -1) == i; } });
        ta.join(); tb.join();
        ok (aOk && bOk, "two RMW writers on different keys never lose each other's key (layer 1: in-process)");
        ok ((int) s.get ("ka", -1) == 99 && (int) s.get ("kb", -1) == 99, "both keys survive with their last values");

        // Layer 2 (cross-process): a real second PROCESS hammers "kc" while we hammer "kp" —
        // fcntl is what excludes it, the in-process mutex cannot (crew finding: the two-thread
        // test alone can pass with only one layer on some platforms).
        {
            const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
            juce::ChildProcess child;
            const bool started = child.start (juce::StringArray { exe.getFullPathName(), "--settings-child", tmp.getFullPathName() });
            ok (started, "child process starts");
            if (started)
            {
                bool pOk = true;
                for (int i = 0; i < 100; ++i) { pOk = pOk && s.set ("kp", i); pOk = pOk && (int) s.get ("kp", -1) == i; }
                const bool childDone = child.waitForProcessToFinish (30000);
                ok (childDone && child.getExitCode() == 0, "child's writes all survived (cross-process RMW)");
                ok (pOk, "parent's writes all survived while the child hammered");
                ok ((int) s.get ("kc", -1) == 99 && (int) s.get ("kp", -1) == 99,
                    "both processes' keys hold their last values");
            }
        }
        tmp.deleteRecursively();
    }

    // VersionBadge / PerfBadge / CallOut — GUI headers: compile-gated under the consumer flag set
    // and smoke-constructed headless (components are never added to the desktop, no popups shown).
    {
        ChkAdapter c;
        felitronics::appkit::VersionBadge badge (c,
            { .productName   = "Appkit Gate",
              .productUrl    = "https://example.invalid/appkit-gate",
              .gitHash       = "deadbee",
              .buildNumber   = 20260712000000LL,
              .buildCount    = 7,
              .gitDirty      = true,
              .os = "macOS", .arch = "arm64", .builder = "gate",
              .coreVersion   = "v0.1.0 (local)",
              .coreOwnerRepo = "darwinscat/felitronics-core" },
            "Standalone");
        ok (badge.getTooltip().startsWith ("Appkit Gate v") && badge.getTooltip().contains ("(Standalone)"),
            "version badge tooltip derives from Config + checker");

        felitronics::appkit::PerfBadge perf ({ .rows = { { "Stage A", felitronics::appkit::brand::violet },
                                                         { "Stage B", felitronics::appkit::brand::orange } } });
        felitronics::appkit::PerfBadge::Stats st;
        st.latencySamples = 64; st.latencyMs = 1.33f; st.total = 42.0f; st.stages = { 12.0f, 30.0f };
        perf.setStats (st);
        ok (perf.getStats().latencySamples == 64 && perf.getStats().stages.size() == 2,
            "perf badge stats snapshot roundtrips");

        auto* callout = &felitronics::appkit::launchCallOut;   (void) callout;
    }

    {
        ChkAdapter c;
        ok (c.releasesPageUrl() == "https://github.com/darwinscat/felitronics-appkit/releases/latest", "releasesPageUrl derives from the slug");
        ok (c.ownerRepo() == "darwinscat/felitronics-appkit", "ownerRepo echoes Config (badge link base)");
        ok (c.currentVersion() == "v0.1.0-7-gdeadbee", "currentVersion echoes Config");
        ok (! c.updateAvailable(), "null settings -> no stored badge");
        ok (c.storedLatest().isEmpty(), "null settings -> empty storedLatest");

        if (live)
        {
            bool done = false;
            c.checkNow ([&done] (felitronics::appkit::UpdateChecker::Result r)
            {
                std::printf ("live: ok=%d outdated=%d latest='%s' url='%s' notes='%s'\n",
                             (int) r.ok, (int) r.outdated, r.latest.toRawUTF8(), r.url.toRawUTF8(), r.notes.toRawUTF8());
                done = true;
            });
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            while (! done && juce::Time::getMillisecondCounterHiRes() - t0 < 15000.0)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            ok (done, "live check delivered a callback");
        }
    }   // dtor: join-never-started (or just-finished) worker — must not hang or assert

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
    std::printf (failures == 0 ? "CONSUMER COMPILE CHECK PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}

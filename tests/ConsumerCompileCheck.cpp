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
#include <felitronics/appkit/NotifyPing.h>
#include <felitronics/appkit/SettingsStore.h>
#include <felitronics/appkit/TextPrompt.h>
#include <felitronics/appkit/UpdateChecker.h>

#include <cstdio>
#include <cstring>

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
        auto* mark = &felitronics::appkit::brand::drawOrbit;   (void) mark;
        auto* prompt = &felitronics::appkit::textPrompt;       (void) prompt;
        ok (felitronics::appkit::brand::violet != felitronics::appkit::brand::orange,
            "brand palette is distinct");
    }

    {
        ChkAdapter c;
        ok (c.releasesPageUrl() == "https://github.com/darwinscat/felitronics-appkit/releases/latest", "releasesPageUrl derives from the slug");
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

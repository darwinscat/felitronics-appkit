// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <felitronics/appkit/Brand.h>
#include <felitronics/appkit/CallOut.h>
#include <felitronics/appkit/UpdateChecker.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include <felitronics/appkit/VersionBadge.h>
#include <felitronics/appkit/PerfBadge.h>
#undef private
#if defined (__clang__)
 #pragma clang diagnostic pop
#endif

#include <cstdio>

using namespace felitronics::appkit;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

struct BadgeChecker final : UpdateChecker
{
    explicit BadgeChecker (juce::String current)
        : UpdateChecker ({ .ownerRepo      = "darwinscat/felitronics-appkit",
                           .productName    = "Badge Gate",
                           .currentVersion = std::move (current),
                           .settings       = {} }) {}
};

static VersionBadge::Config versionConfig (juce::String coreVersion)
{
    VersionBadge::Config cfg;
    cfg.productName = "Badge Gate";
    cfg.productUrl = "https://example.invalid/badge-gate";
    cfg.gitHash = "deadbee";
    cfg.buildNumber = 20260712000000LL;
    cfg.buildCount = 7;
    cfg.gitDirty = true;
    cfg.os = "macOS";
    cfg.arch = "arm64";
    cfg.builder = "gate";
    cfg.coreVersion = std::move (coreVersion);
    cfg.coreOwnerRepo = "darwinscat/felitronics-core";
    return cfg;
}

static int paintedAlphaPixels (juce::Component& c)
{
    c.resized();
    juce::Image img (juce::Image::ARGB, juce::jmax (1, c.getWidth()), juce::jmax (1, c.getHeight()), true);
    juce::Graphics g (img);
    c.paintEntireComponent (g, true);

    int n = 0;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt (x, y).getAlpha() != 0)
                ++n;
    return n;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("felitronics::appkit badges falsification tests\n");

    group ("VersionBadge canonicalizes currentVersion labels and release URLs");
    {
        ok (VersionBadge::displayVersionTag ("1.2.3") == "v1.2.3", "bare semver gets one display v");
        ok (VersionBadge::displayVersionTag ("v1.2.3") == "v1.2.3", "v-prefixed semver is not double-prefixed");
        ok (VersionBadge::displayVersionTag ("V1.2.3") == "v1.2.3", "upper-case V is canonicalized");
        ok (VersionBadge::displayVersionTag ("unknown") == "unknown", "non-numeric dev stamp is not v-prefixed");
        ok (VersionBadge::releaseTagForCurrentVersion ("v0.1.0-7-gdeadbee") == "v0.1.0",
            "git-describe version links to its base release tag");
        ok (VersionBadge::releaseTagForCurrentVersion ("0.1.0+build5") == "v0.1.0",
            "build-metadata version links to its base release tag");

        BadgeChecker c ("v0.1.0-7-gdeadbee");
        const auto cfg = versionConfig ("v0.8.0 (local)");
        VersionBadge badge (c, cfg, "Standalone");
        ok (badge.getTooltip().contains ("Badge Gate v0.1.0-7-gdeadbee"), "tooltip keeps a single v-prefixed stamp");
        ok (! badge.getTooltip().contains ("vv0.1.0"), "tooltip never shows the double-v fixture quirk");

        VersionBadge::Panel panel (badge, cfg, "Standalone", nullptr);
        ok (panel.verLink.getButtonText() == "v0.1.0-7-gdeadbee", "popover version text keeps the full build stamp");
        ok (panel.verLink.getURL().toString (false).endsWith ("/releases/tag/v0.1.0"),
            "popover version URL points at the base release tag");
    }

    group ("VersionBadge panel handles empty coreVersion sizing and copied config lifetime");
    {
        BadgeChecker c ("1.2.3");
        const auto noCore = versionConfig ({});
        VersionBadge badge (c, noCore, "Standalone");
        VersionBadge::Panel panel (badge, noCore, "Standalone", nullptr);
        ok (panel.getWidth() == 300 && panel.getHeight() == 232, "empty coreVersion removes exactly one popup row");
        ok (panel.coreLink.getParentComponent() == nullptr, "empty coreVersion does not attach a core link");
        ok (paintedAlphaPixels (panel) > 0, "empty-core panel paints nonblank headless content");

        const auto withCore = versionConfig ("v0.8.0-3-gabc1234");
        VersionBadge badge2 (c, withCore, "Standalone");
        VersionBadge::Panel panel2 (badge2, withCore, "Standalone", nullptr);
        ok (panel2.getWidth() == 300 && panel2.getHeight() == 248, "non-empty coreVersion keeps the dependency row");
        ok (panel2.coreLink.getParentComponent() == &panel2, "non-empty coreVersion attaches a core link");
    }

    group ("VersionBadge SafePointer paths no-op after the owner badge is gone");
    {
        BadgeChecker c ("1.2.3");
        const auto cfg = versionConfig ({});
        auto badge = std::make_unique<VersionBadge> (c, cfg, "Standalone");
        VersionBadge::Panel panel (*badge, cfg, "Standalone", nullptr);
        badge.reset();
        panel.runCheck();
        ok (! panel.check.isEnabled(), "check button disables instead of calling through a dead owner");
        panel.onResult ({});
        ok (panel.result.getText().isNotEmpty(), "late result handling remains safe with a dead owner");
    }

    group ("PerfBadge zero-row panel is live and safe after owner deletion");
    {
        auto perf = std::make_unique<PerfBadge> (PerfBadge::Config { .rows = {} });
        PerfBadge::Stats st;
        st.latencySamples = 0;
        st.latencyMs = 0.0f;
        st.total = 123.0f;
        st.stages = { 99.0f };
        perf->setStats (st);

        PerfBadge::Panel panel (*perf);
        ok (panel.getWidth() == 236 && panel.getHeight() == 98, "zero stage rows use only fixed popup chrome");
        ok (paintedAlphaPixels (panel) > 0, "zero-row PerfBadge panel paints nonblank content");
        perf.reset();
        ok (paintedAlphaPixels (panel) > 0, "zero-row PerfBadge panel paints safely after owner deletion");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

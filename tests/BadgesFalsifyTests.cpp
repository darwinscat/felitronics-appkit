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
        ok (panel.rows.size() == 2 && panel.rows[0]->link.getButtonText() == "v0.1.0",
            "the product row shows the RELEASE, not the describe tail");
        ok (panel.rows[0]->link.getURL().toString (false).endsWith ("/releases/tag/v0.1.0"),
            "the product row links to its base release tag");
    }

    group ("VersionBadge table: the dev facts are columns, not suffixes on the version");
    {
        const auto loc   = VersionBadge::Panel::splitStamp ("v0.24.0 (local)");
        const auto desc  = VersionBadge::Panel::splitStamp ("v0.11.3-4-g6b06cba");
        const auto dirty = VersionBadge::Panel::splitStamp ("v0.6.0-dirty");
        const auto plain = VersionBadge::Panel::splitStamp ("8.0.14");
        ok (loc.version == "v0.24.0" && loc.state == "local",
            "a sibling checkout reads as a state, not as part of the version");
        ok (desc.version == "v0.11.3" && desc.commit == "g6b06cba",
            "a describe tail splits into version + commit");
        ok (dirty.version == "v0.6.0" && dirty.state == "dirty",
            "an uncommitted tree reads as a state, not as part of the version");
        ok (plain.version == "8.0.14" && plain.state.isEmpty(), "a plain release number stays exactly itself");
        ok (VersionBadge::prettyBuildStamp (7) == "7", "a stamp that isn't fourteen digits passes through raw");

        BadgeChecker c ("1.2.3");
        auto cfg = versionConfig ("v0.8.0 (local)");
        cfg.licence = "AGPL-3.0-or-later";
        cfg.dependencies = { { "felitronics-appkit", "v0.11.4", "darwinscat/felitronics-appkit", {}, {} },
                             { "JUCE", "8.0.14", {}, "gfeedface", "pin" } };
        VersionBadge badge (c, cfg, "Standalone");
        VersionBadge::Panel panel (badge, cfg, "Standalone", nullptr);

        ok (panel.rows.size() == 4, "the product leads, then the legacy core, then the listed deps");
        ok (panel.rows[0]->lead.getText().startsWith ("Badge Gate"), "row 0 is the product itself");
        ok (panel.rows[0]->state.getText() == "dirty"
                && panel.rows[0]->commitLink.getButtonText() == "gdeadbee"
                && panel.rows[0]->built.getText() == "2026-07-12 00:00:00",
            "the product's state, commit and build clock live in their own columns");
        ok (panel.rows[0]->commitLink.getURL().toString (false).endsWith ("/commit/deadbee")
                && panel.rows[0]->commit.getText().isEmpty(),
            "a row with a slug links its hash to that commit, and leaves the plain cell empty");
        ok (! panel.rows[0]->link.getButtonText().contains ("dirty"), "...and never in its version cell");
        ok (panel.rows[1]->state.getText() == "local", "a sibling dependency says so in the state column");
        ok (panel.rows[3]->state.getText() == "pin" && panel.rows[3]->commit.getText() == "gfeedface"
                && panel.rows[3]->commitLink.getParentComponent() == nullptr,
            "a dependency may state its commit and origin outright, where the version string cannot"
            " — and without a slug the hash stays plain text, like the version beside it");
        ok (panel.rows[3]->built.getText().isEmpty(), "a row with nothing to say in a column leaves it empty");
        ok (panel.rows[2]->link.getButtonText() == "v0.11.4"
                && panel.rows[2]->link.getURL().toString (false).endsWith ("/releases/tag/v0.11.4"),
            "a slug turns the version into a link to its release tag");
        ok (panel.rows[3]->link.getParentComponent() == nullptr
                && panel.rows[3]->plain.getText() == "8.0.14",
            "a dependency without a slug stays plain text");
        ok (panel.rows[0]->lead.getText() == "Badge Gate" && panel.rows[3]->lead.getText() == "JUCE",
            "a cell holds its own text — the columns do the aligning, not padding");
        ok (panel.licence.getText() == "AGPL-3.0-or-later", "the licence has its own row under the table");
        panel.resized();
        ok (panel.link.getURL().toString (false) == "https://darwinscat.com"
                && panel.productHit.url.toString (false) == "https://example.invalid/badge-gate",
            "the title's two halves lead where each is pointing: the name home to the product, the "
            "byline home to the maker");
        ok (panel.productHit.getBounds().contains (panel.markArea)
                && ! panel.productHit.getBounds().intersects (panel.link.getBounds()),
            "the product's hit area covers its own mark and stops before the byline's");

        ok (panel.site.getWidth() > (int) VersionBadge::Panel::kTextH * 4
                && panel.site.getRight() <= panel.getWidth()
                && panel.site.getButtonText().startsWith ("https://"),
            "the address is laid out on the licence row, with its scheme, and not at zero width");
        ok (panel.note.getText().contains ("GitHub") && panel.note.getText().contains ("Nothing is sent to us")
                && ! panel.note.getText().containsIgnoreCase ("opt-in"),
            "the small print says where the request goes and what comes back, not jargon");
        ok (panel.note.getTooltip().contains ("IP address") && panel.autoCheck.getTooltip() == panel.note.getTooltip(),
            "the part a lawyer cares about is one hover away, on the note and on the switch");
        ok (panel.env.getText().contains ("Standalone") && panel.env.getText().contains ("arm64")
                && panel.env.getText().contains ("gate"),
            "the environment row carries format, machine and builder");

        ok (panel.stampText.startsWith ("Badge Gate"), "the copy leads with the product row");
        ok (panel.stampText.contains ("gdeadbee") && panel.stampText.contains ("local")
                && panel.stampText.contains ("JUCE") && panel.stampText.contains ("AGPL-3.0-or-later"),
            "the copy is the whole table plus the rows beneath it");

        panel.resized();
        ok (panel.stampArea.contains (panel.rows[0]->lead.getBounds())
                && panel.stampArea.contains (panel.rows[3]->lead.getBounds()),
            "the box covers every row it copies");
        ok (panel.rows[0]->lead.getX() == panel.rows[3]->lead.getX()
                && panel.rows[0]->state.getX() == panel.rows[1]->state.getX(),
            "the cells sit on real columns, not on padded text");
        ok (panel.rows[0]->built.getRight() <= panel.stampArea.getRight(),
            "the widest column still fits inside the table's box");
        ok (! panel.stampArea.contains (panel.check.getBounds()), "the update button stays outside the copy target");
        ok (! panel.copied, "the panel opens showing the invitation, not the receipt");

        juce::SystemClipboard::copyTextToClipboard ("something else entirely");
        panel.copyStamp();
        ok (juce::SystemClipboard::getTextFromClipboard() == panel.stampText,
            "the copy affordance actually puts the table on the clipboard");
        ok (panel.copied && panel.copyBtn.getButtonText() != "copy build stamp",
            "...and says so on the button");
        panel.copiedFrames = 1;
        panel.timerCallback();
        ok (! panel.copied && panel.copyBtn.getButtonText() == juce::String ("copy build stamp"),
            "the receipt reverts to the invitation");
        panel.showUpdate ("9.9.9", juce::URL ("https://example.invalid/rel"));
        ok (panel.download.isVisible() && ! panel.result.isVisible()
                && panel.download.getButtonText().contains ("9.9.9")
                && panel.download.getButtonText().contains ("Download"),
            "an available update turns the whole verdict into the link");
        ok (panel.download.getButtonText().contains ("Update available"),
            "a release build is told an update is available");

        BadgeChecker dev ("v0.1.0-7-gdeadbee");            // a working-tree build, not a release
        VersionBadge devBadge (dev, cfg, "Standalone");
        VersionBadge::Panel devPanel (devBadge, cfg, "Standalone", nullptr);
        devPanel.showUpdate ("0.1.0", juce::URL ("https://example.invalid/rel"));
        ok (! devPanel.isRelease && devPanel.download.getButtonText().startsWith ("Latest release"),
            "a working-tree build is told a release EXISTS, not that its own version updates itself");
        ok (panel.download.getWidth() > panel.getWidth() / 2,
            "...and that link spans the row, not a word at its end");

        bool wantsClicks = true, wantsChildClicks = true;
        panel.rows[0]->lead.getInterceptsMouseClicks (wantsClicks, wantsChildClicks);
        ok (! wantsClicks, "a table cell lets the click through to the copy target beneath it");
        ok (panel.rows[0]->link.getWidth() > (int) VersionBadge::Panel::kTextH * 3
                && panel.rows[1]->link.getWidth() >= panel.rows[0]->link.getWidth(),
            "a version cell is wide enough for the link that draws in it");
        ok (panel.feed.getWidth() > (int) VersionBadge::Panel::kFeedH * 3,
            "the tip jar's words get a width, not the zero a HyperlinkButton is born with");
        ok (panel.getWidth() >= VersionBadge::Panel::kMinWidth,
            "the window never gets narrower than its floor, whatever the table holds");
    }

    group ("VersionBadge panel sizing follows the table it carries");
    {
        BadgeChecker c ("1.2.3");
        const auto noCore = versionConfig ({});
        VersionBadge badge (c, noCore, "Standalone");
        VersionBadge::Panel panel (badge, noCore, "Standalone", nullptr);
        ok (panel.rows.size() == 1, "empty coreVersion attaches no dependency row");
        ok (paintedAlphaPixels (panel) > 0, "panel paints nonblank headless content");

        const auto withCore = versionConfig ("v0.8.0-3-gabc1234");
        VersionBadge badge2 (c, withCore, "Standalone");
        VersionBadge::Panel panel2 (badge2, withCore, "Standalone", nullptr);
        ok (panel2.getHeight() == panel.getHeight() + 18, "the legacy core adds exactly one row");
        ok (panel2.rows.size() == 2 && panel2.rows[1]->link.getParentComponent() == &panel2,
            "the legacy core row is linked like any other");
    }

    group ("VersionBadge feed row: on by default at the canonical URL, gone when cleared");
    {
        BadgeChecker c ("1.2.3");
        const auto cfg = versionConfig ({});
        VersionBadge badge (c, cfg, "Standalone");

        VersionBadge::Panel panel (badge, cfg, "Standalone", nullptr);
        ok (panel.feed.getParentComponent() == &panel, "default config attaches the feed link");
        ok (panel.feed.getButtonText() == "Feed the Cat", "feed link carries the default label");
        ok (panel.feedPrompt.getParentComponent() == &panel && panel.feedPrompt.getText() == "Like the app?",
            "the default block leads with the quiet prompt line");
        {
            const juce::String fed = panel.feed.getURL().toString (true);
            ok (fed.startsWith (juce::String (brand::feedTheCatUrl) + "?from=badgegate")
                    && fed.contains ("platform=") && fed.contains ("format=standalone"),
                "feed link opens the canonical hop, signed with product, machine and wrapper");
        }
        panel.resized();
        ok (! panel.pawArea.isEmpty(), "a visible feed row lays out a paw to draw");
        bool pawClicks = false, pawChildClicks = false;
        panel.pawBtn.getInterceptsMouseClicks (pawClicks, pawChildClicks);
        ok (panel.feedUrl == panel.feed.getURL() && panel.pawBtn.isVisible() && pawClicks,
            "the print is a click target of its own, opening what the words open");
        ok (panel.pawBtn.getBounds().contains (panel.pawArea),
            "...and its target covers the print and the halo around it");
        ok (panel.note.getY() - panel.check.getBottom() <= 2,
            "the telemetry note hugs the update button it describes");
        ok (panel.note.getBottom() <= panel.feedPrompt.getY(),
            "the telemetry note clings to the update block, not to the cat");
        ok (panel.feedPrompt.getRight() <= panel.pawArea.getX()
                && panel.pawArea.getRight() <= panel.feed.getX(),
            "the prompt leads, then the paw, then the words — one sentence, one row");
        ok (paintedAlphaPixels (panel) > 0, "feed-row panel paints nonblank headless content");

        auto muted = cfg;
        muted.feedUrl = {};
        VersionBadge badge2 (c, muted, "Standalone");
        VersionBadge::Panel panel2 (badge2, muted, "Standalone", nullptr);
        ok (panel2.feed.getParentComponent() == nullptr, "an empty feedUrl attaches no feed link");
        ok (panel2.feedPrompt.getParentComponent() == nullptr, "an empty feedUrl drops the prompt line too");
        ok (panel2.getHeight() == panel.getHeight() - VersionBadge::Panel::kFeedRowH,
            "an empty feedUrl shrinks the popup back by the whole block");

        auto noPrompt = cfg;
        noPrompt.feedPrompt = {};
        VersionBadge badge3 (c, noPrompt, "Standalone");
        VersionBadge::Panel panel3 (badge3, noPrompt, "Standalone", nullptr);
        ok (panel3.feed.getParentComponent() == &panel3 && panel3.feedPrompt.getParentComponent() == nullptr,
            "an empty feedPrompt keeps the paw row and drops only the prompt");
        ok (panel3.getHeight() == panel.getHeight(),
            "the prompt shares the paw's row, so dropping it costs no height");
        panel2.resized();
        ok (panel2.pawArea.isEmpty(), "no feed row, no paw");
    }

    group ("brand::feedTheCatLink signs the hop with a folded product slug");
    {
        // The platform is compiled in, so the tests name it the same way the header does rather than
        // hard-coding one OS and failing on the other two gates.
       #if JUCE_MAC
        const juce::String platform = "platform=macos";
       #elif JUCE_WINDOWS
        const juce::String platform = "platform=windows";
       #elif JUCE_LINUX
        const juce::String platform = "platform=linux";
       #else
        const juce::String platform = "platform=other";
       #endif

        ok (brand::feedTheCatLink ("LooperCat").toString (true)
                == juce::String (brand::feedTheCatUrl) + "?from=loopercat&" + platform,
            "product name folds to a bare lowercase slug, and the machine signs beside it");
        ok (brand::feedTheCatLink ("Badge Gate 2!").toString (true)
                == juce::String (brand::feedTheCatUrl) + "?from=badgegate2&" + platform,
            "spaces and punctuation drop out of the slug");
        ok (brand::feedTheCatLink ("\u00e9\u00e9").toString (true)
                == juce::String (brand::feedTheCatUrl) + "?" + platform,
            "a name with no sluggable characters leaves the hop unsigned, but still says where from");
        ok (brand::feedTheCatLink ("LooperCat", "https://example.invalid/hop").toString (true)
                == "https://example.invalid/hop?from=loopercat&" + platform,
            "a custom base keeps the signature");
        ok (brand::feedTheCatLink ("LooperCat", brand::feedTheCatUrl, "VST3").toString (true)
                == juce::String (brand::feedTheCatUrl) + "?from=loopercat&" + platform + "&format=vst3",
            "a known wrapper signs as a third column, folded like the slug");
        ok (! brand::feedTheCatLink ("LooperCat", brand::feedTheCatUrl, "").toString (true).contains ("format="),
            "an unknown wrapper adds no empty column");
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

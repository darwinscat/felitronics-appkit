// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <felitronics/appkit/Brand.h>           // brand::drawOrbit — the family-default title mark
#include <felitronics/appkit/CallOut.h>         // launchCallOut — editor-parented CallOutBox
#include <felitronics/appkit/UpdateChecker.h>   // the opt-in GitHub-release check this badge fronts

#include <functional>
#include <memory>
#include <utility>

//==============================================================================
// felitronics::appkit::VersionBadge — a small clickable "v1.6.0 / <format>" label, bottom-right.
// Always shown, offline-safe. A bright static accent dot appears when a stored "latest" is newer
// than the installed version. Click → a CallOutBox popup with the brand mark, the full build stamp
// (with GitHub links for the version / commit / core), and an opt-in "Check for updates" button
// (the ONLY thing that hits the network — never silent). Extracted verbatim from OrbitCab's
// ui/VersionBadge.h; everything product-specific moved into Config.
//
// The checker reference is the product's thin UpdateChecker adapter (see UpdateChecker.h) — the
// badge derives its repo links from the SAME slug the checker queries, so they can never drift.
// The checker (and the Config's captured state, if any) must outlive the badge.
//==============================================================================
namespace felitronics::appkit
{

class VersionBadge final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    //==========================================================================
    // Everything product-specific lives here; the badge itself is product-blind. The GitHub slug
    // comes from the UpdateChecker (ownerRepo()), the running version from currentVersion() — this
    // struct carries only what the checker doesn't know: identity, build stamp, dependency line.
    struct Config
    {
        // Product display name — the popover's wordmark text and the tooltip prefix. REQUIRED.
        juce::String productName;

        // The brand byline link under the title mark: its text and click-through (the product's
        // landing page, e.g. "https://example.com/product?utm_source=product&utm_medium=plugin").
        // productUrl is REQUIRED; an empty byline hides nothing — supply both.
        juce::String byline = "by Darwin's Cat";
        juce::String productUrl;

        // The build stamp. Each product bakes its own generated version header at build time
        // (end users have no git repo) — pass those constants through here.
        juce::String gitHash;              // short HEAD hash → the "g<hash>" line linking to /commit/<hash>
        juce::int64  buildNumber = 0;      // UTC YYYYMMDDHHMMSS → the "build N" annotation
        int          buildCount  = 0;      // commits since the release tag → "· N commits" when > 0
        bool         gitDirty    = false;  // uncommitted tracked changes → "· dirty"
        juce::String os, arch, builder;    // the environment line: "<format> · <os> <arch> · <builder>"

        // Optional dependency line, e.g. "core v0.8.0 (local)" linking to that repo's release tag.
        // coreVersion is a resolved stamp ("v0.8.0", "v0.8.0 (local)", "v0.8.0-3-g1a2b3c4"): the
        // link strips any " …" / "-N-g…" suffix down to the bare vX.Y.Z tag and the suffix stays as
        // plain trailing text. An EMPTY coreVersion hides the whole row (the popup shrinks by one line).
        juce::String coreLabel = "core ";
        juce::String coreVersion;
        juce::String coreOwnerRepo;        // GitHub "<owner>/<repo>" slug for the dependency link

        // The popover's title mark, drawn left of the wordmark (centre cx,cy / diameter d). Null →
        // the family-default appkit brand::drawOrbit. Products with their own mark variant pass it
        // here so the popover mirrors their window header exactly.
        std::function<void (juce::Graphics&, float cx, float cy, float d)> drawMark;

        // Visual defaults — OrbitCab's current pixels (its LookAndFeel constants, which differ
        // from the brand:: palette on purpose: the badge predates the consolidated identity).
        juce::Colour accent      { 0xff7c4dff };   // the format line
        juce::Colour accentHover { 0xff9778ff };   // version when outdated; hyperlink text
        juce::Colour accentB     { 0xffff8822 };   // the "new" dot; the update line + Download link
        juce::Colour text        { 0xffd8d8d8 };   // the popover wordmark
    };

    VersionBadge (UpdateChecker& uc, Config cfg, juce::String pluginFormat)
        : checker (uc), config (std::move (cfg)), format (std::move (pluginFormat))
    {
        jassert (config.productName.isNotEmpty());   // no name, no badge
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip (config.productName + " v" + checker.currentVersion() + " (" + format + ") — click to check for updates");
    }

    // The editor supplies the embedded brand typeface (loaded once for the header) so the popover's
    // title mark matches the window header. Call after construction, before the first popup.
    // Bold system fallback if never set / null.
    void setBrandTypeface (juce::Typeface::Ptr tf) { brandTypeface = std::move (tf); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const bool upd = checker.updateAvailable();

        // Line 1 — the version, a touch bigger; line 2 (below) is the running plugin format.
        auto verRow = r.removeFromTop (r.getHeight() * 0.56f);
        const juce::Font verFont (juce::FontOptions (14.0f, juce::Font::bold));
        const juce::String ver = "v" + checker.currentVersion();
        g.setFont (verFont);
        g.setColour (upd ? config.accentHover : juce::Colour (0xff8a8a92));
        g.drawText (ver, verRow, juce::Justification::centredLeft, false);

        // Line 2 — the running plugin format (VST3 / AU / CLAP / Standalone). The build number
        // lives only in the (i) popover — the corner stays version + format.
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.setColour (config.accent.withAlpha (0.9f));
        g.drawText (format, r, juce::Justification::centredLeft, false);

        if (upd)   // bright static dot just right of the version (update available)
        {
            const float tw = textWidth (verFont, ver);
            const float cx = verRow.getX() + tw + 7.0f, cy = verRow.getCentreY();
            g.setColour (config.accentB);   // hot accent = "new"
            g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()))
            showPopup();
    }

private:
    // Rendered width of `s` in font `f` (dot position / hand layout in the popover).
    static float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    //--- the CallOutBox content ----------------------------------------------
    struct Panel final : public juce::Component
    {
        Panel (UpdateChecker& uc, VersionBadge& ownerBadge, Config cfg,
               juce::String pluginFormat, juce::Typeface::Ptr brandTf)
            : checker (uc), owner (&ownerBadge), config (std::move (cfg)), brandTypeface (std::move (brandTf))
        {
            const juce::String mono = juce::Font::getDefaultMonospacedFontName();
            const juce::String mid  = juce::String::fromUTF8 (" \xc2\xb7 ");   // " · "
            const bool hasCore = config.coreVersion.isNotEmpty();

            // The brand byline link (under the title mark).
            link.setButtonText (config.byline);
            link.setURL (juce::URL (config.productUrl));
            link.setColour (juce::HyperlinkButton::textColourId, config.accentHover);
            link.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (link);

            // The GitHub links (version → release tag, commit → HEAD, core → the dependency's tag).
            auto ghLink = [&] (juce::HyperlinkButton& b, const juce::String& text, const juce::String& url)
            {
                b.setButtonText (text);
                b.setURL (juce::URL (url));
                b.setFont (juce::FontOptions (11.0f).withName (mono), false, juce::Justification::centredLeft);
                b.setColour (juce::HyperlinkButton::textColourId, config.accentHover);
                b.changeWidthToFitText();
                addAndMakeVisible (b);
            };
            const juce::String repoBase = "https://github.com/" + checker.ownerRepo();
            const juce::String ver = "v" + checker.currentVersion();
            ghLink (verLink,    ver,                                 repoBase + "/releases/tag/" + ver);
            ghLink (commitLink, juce::String ("g") + config.gitHash, repoBase + "/commit/" + config.gitHash);
            if (hasCore)
            {
                // core: strip " (local)" and any "-N-g…" dev suffix → the bare vX.Y.Z release tag.
                const juce::String coreTag = config.coreVersion.upToFirstOccurrenceOf (" ", false, false)
                                                               .upToFirstOccurrenceOf ("-", false, false);
                ghLink (coreLink, coreTag, "https://github.com/" + config.coreOwnerRepo + "/releases/tag/" + coreTag);
            }

            // The plain-text bits that annotate each link line.
            auto info = [&] (juce::Label& l, const juce::String& text)
            {
                l.setText (text, juce::dontSendNotification);
                l.setFont (juce::FontOptions (11.0f).withName (mono));
                l.setJustificationType (juce::Justification::centredLeft);
                l.setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa4));
                addAndMakeVisible (l);
            };
            juce::String tailAtxt;                                   // annotates the version line
            if (config.buildCount > 0) tailAtxt << mid << config.buildCount << " commits";
            if (config.gitDirty)       tailAtxt << mid << "dirty";
            info (tailA, tailAtxt);
            info (tailB, juce::String ("  build ") + juce::String (config.buildNumber));   // annotates the commit line
            info (line3, pluginFormat + mid + config.os + " " + config.arch + mid + config.builder);
            if (hasCore)
            {
                info (coreLead, config.coreLabel);
                info (coreTail, config.coreVersion.fromFirstOccurrenceOf (" ", true, false));   // " (local)" or ""
            }

            check.setButtonText ("Check for updates");
            check.onClick = [this] { runCheck(); };
            addAndMakeVisible (check);

            result.setFont (juce::FontOptions (12.0f));
            result.setJustificationType (juce::Justification::centredLeft);
            result.setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa4));
            addAndMakeVisible (result);

            download.setButtonText ("Download");
            download.setColour (juce::HyperlinkButton::textColourId, config.accentB);
            addChildComponent (download);   // hidden until an update is actually available (then setURL + setVisible)

            note.setText ("Opt-in. Sends only product + version.", juce::dontSendNotification);
            note.setFont (juce::FontOptions (10.0f));
            note.setColour (juce::Label::textColourId, juce::Colour (0xff60606a));
            addAndMakeVisible (note);

            // If an update is already known from a previous check, show it up front.
            if (checker.updateAvailable())
                showUpdate (checker.storedLatest(), juce::URL (checker.releasesPageUrl()));

            setSize (300, hasCore ? 248 : 232);   // one 16 px row less without the dependency line
        }

        // Brand title: [mark] <productName>, mirroring the window header. Drawn (not a Label) so
        // the mark + wordmark share the product's exact renderer (Config::drawMark + the typeface).
        void paint (juce::Graphics& g) override
        {
            const auto a = titleArea.toFloat();
            const float d  = a.getHeight() * 0.92f;
            const float cy = a.getCentreY();
            if (config.drawMark != nullptr)
                config.drawMark (g, a.getX() + d * 0.5f, cy, d);
            else
                brand::drawOrbit (g, a.getX() + d * 0.5f, cy, d);

            const auto wf = brandTypeface != nullptr
                              ? juce::Font (juce::FontOptions().withHeight (a.getHeight() * 0.66f).withTypeface (brandTypeface))
                              : juce::Font (juce::FontOptions (a.getHeight() * 0.66f, juce::Font::bold));
            g.setFont (wf);
            g.setColour (config.text);
            const float baseline = cy + (wf.getAscent() - wf.getDescent()) * 0.5f;
            g.drawSingleLineText (config.productName, juce::roundToInt (a.getX() + d + 7.0f), juce::roundToInt (baseline));
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14, 12);
            titleArea = r.removeFromTop (26);           // [mark] <productName> — drawn in paint()
            link.setBounds (r.removeFromTop (16));
            r.removeFromTop (5);

            // Info rows: a GitHub link (fitted width) + a trailing plain label.
            auto rowV = r.removeFromTop (16);
            verLink.setBounds    (rowV.removeFromLeft (verLink.getWidth()));
            tailA.setBounds      (rowV);
            auto rowC = r.removeFromTop (16);
            commitLink.setBounds (rowC.removeFromLeft (commitLink.getWidth()));
            tailB.setBounds      (rowC);
            line3.setBounds      (r.removeFromTop (16));
            if (config.coreVersion.isNotEmpty())
            {
                auto rowK = r.removeFromTop (16);
                coreLead.setBounds (rowK.removeFromLeft ((int) textWidth (coreLead.getFont(), coreLead.getText()) + 3));
                coreLink.setBounds (rowK.removeFromLeft (coreLink.getWidth()));
                coreTail.setBounds (rowK);
            }

            r.removeFromTop (7);
            check.setBounds    (r.removeFromTop (26));
            r.removeFromTop (4);
            result.setBounds   (r.removeFromTop (18));
            download.setBounds (r.removeFromTop (16));
            note.setBounds     (r.removeFromBottom (14));
        }

        void runCheck()
        {
            check.setEnabled (false);
            result.setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa4));
            result.setText (juce::String::fromUTF8 ("Checking\xe2\x80\xa6"), juce::dontSendNotification);
            download.setVisible (false);

            juce::Component::SafePointer<Panel> safe (this);
            checker.checkNow ([safe] (UpdateChecker::Result res)
            {
                if (auto* self = safe.getComponent())
                    self->onResult (res);
            });
        }

        void onResult (const UpdateChecker::Result& res)
        {
            check.setEnabled (true);
            if (owner != nullptr) owner->repaint();   // badge dot may have appeared/cleared (badge may be gone)

            if (! res.ok)
            {
                result.setColour (juce::Label::textColourId, juce::Colour (0xffb0b0b8));
                result.setText (juce::String::fromUTF8 ("Couldn\xe2\x80\x99t check (offline?)"), juce::dontSendNotification);
                return;
            }
            if (res.outdated)
                showUpdate (res.latest, juce::URL (res.url.isNotEmpty() ? res.url : checker.releasesPageUrl()));
            else
            {
                result.setColour (juce::Label::textColourId, juce::Colour (0xff7be29a));   // green
                result.setText (juce::String::fromUTF8 ("\xe2\x9c\x93 Up to date"), juce::dontSendNotification);
            }
        }

        void showUpdate (const juce::String& latest, const juce::URL& url)
        {
            result.setColour (juce::Label::textColourId, config.accentB);
            result.setText (juce::String::fromUTF8 ("\xe2\x86\x91 Update available: v") + latest, juce::dontSendNotification);
            download.setURL (url);
            download.setVisible (true);
        }

        UpdateChecker& checker;
        juce::Component::SafePointer<VersionBadge> owner;   // the badge may outlive-die before an async check returns
        const Config          config;                       // OWN copy — safe if the badge dies while the popup is open
        juce::Typeface::Ptr   brandTypeface;                // the brand face for the title (from the editor; bold fallback if null)
        juce::Rectangle<int>  titleArea;                    // where paint() draws [mark] <productName>
        juce::Label           result, note, tailA, tailB, line3, coreLead, coreTail;
        juce::HyperlinkButton link, download, verLink, commitLink, coreLink;
        juce::TextButton      check;
    };

    void showPopup()
    {
        launchCallOut (*this, std::make_unique<Panel> (checker, *this, config, format, brandTypeface));
    }

    UpdateChecker&      checker;
    Config              config;
    juce::String        format;          // running plugin format (VST3 / AU / CLAP / Standalone)
    juce::Typeface::Ptr brandTypeface;   // brand face for the popover title (set by the editor)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VersionBadge)
};

} // namespace felitronics::appkit

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

        // An optional SECOND mark, drawn left of the byline row: the family sign (Darwin's Cat)
        // under the product's own, exactly as a product's window header carries both. Null (the
        // default) leaves the byline starting at the left margin.
        std::function<void (juce::Graphics&, float cx, float cy, float d)> drawByline;

        // Visual defaults — OrbitCab's current pixels (its LookAndFeel constants, which differ
        // from the brand:: palette on purpose: the badge predates the consolidated identity).
        juce::Colour accent      { 0xff7c4dff };   // the format line
        juce::Colour accentHover { 0xff9778ff };   // version when outdated; hyperlink text
        juce::Colour accentB     { 0xffff8822 };   // the "new" dot; the update line + Download link
        juce::Colour text        { 0xffd8d8d8 };   // the popover wordmark

        // The "Feed the cat" block at the popover's foot: a quiet one-line prompt, then a paw
        // print + hyperlink opening the family tip jar. ON by default so every product inherits
        // it with an appkit bump; an empty feedUrl hides the whole block and the popup shrinks
        // back, an empty feedPrompt drops just the prompt line.
        juce::String feedPrompt = "Like the app?";
        juce::String feedLabel  = "Feed the Cat";
        juce::String feedUrl    = brand::feedTheCatUrl;
    };

    VersionBadge (UpdateChecker& uc, Config cfg, juce::String pluginFormat)
        : checker (uc), config (std::move (cfg)), format (std::move (pluginFormat))
    {
        jassert (config.productName.isNotEmpty());   // no name, no badge
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip (config.productName + " " + displayVersionTag (checker.currentVersion()) + " (" + format + ") — click to check for updates");
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
        const juce::String ver = displayVersionTag (checker.currentVersion());
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

    // The popover, cast programmatically. A product may prefer its OWN affordance as the door —
    // TabbyEQ's toolbar (i) button, say — instead of (or beside) a click on the badge itself; then
    // the badge can stay invisible and simply own the config + the checker wiring. Message thread
    // only, like every other entry point here. Defined below the class: the panel it builds is a
    // private nested type.
    void showPopup();

private:
    // Rendered width of `s` in font `f` (dot position / hand layout in the popover).
    static float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    static juce::String displayVersionTag (juce::String version)
    {
        version = version.trim();
        if (version.isEmpty())
            return {};
        if (version.startsWithIgnoreCase ("v"))
            return "v" + version.substring (1);
        const auto first = version[0];
        return first >= '0' && first <= '9' ? "v" + version : version;
    }

    static juce::String releaseTagForCurrentVersion (juce::String version)
    {
        const juce::String tag = displayVersionTag (std::move (version));
        const juce::String base = tag.upToFirstOccurrenceOf ("-", false, false)
                                     .upToFirstOccurrenceOf ("+", false, false);
        return update::isCleanRelease (base.toStdString()) ? base : tag;
    }

    // The 14-digit UTC build stamp (YYYYMMDDHHMMSS) sliced into human blocks:
    // 20260713092444 -> "2026-07-13 09:24:44". The clock is UTC and stays unlabelled — the row is a
    // build stamp, not a local time anyone converts. A stamp that isn't that shape (a generator
    // failure path bakes a 0) is shown raw rather than mangled.
    static juce::String prettyBuildStamp (juce::int64 buildNumber)
    {
        const auto raw = juce::String (buildNumber);
        if (raw.length() != 14)
            return raw;
        return raw.substring (0, 4)  + "-" + raw.substring (4, 6)   + "-" + raw.substring (6, 8)
             + " "
             + raw.substring (8, 10) + ":" + raw.substring (10, 12) + ":" + raw.substring (12, 14);
    }

    //--- the CallOutBox content ----------------------------------------------
    // The callout is a child of the TOP-LEVEL window (see CallOut.h), so it can OUTLIVE the badge —
    // and in a consumer that owns badge + checker together (a closable settings subview, say), the
    // checker dies with them. The panel therefore never stores a checker reference: everything it
    // needs to DRAW is copied at construction (the badge is alive then — it launched us), and the
    // one post-construction checker call (checkNow) reaches it through the live badge or no-ops.
    struct Panel final : public juce::Component,
                         private juce::Timer
    {
        Panel (VersionBadge& ownerBadge, Config cfg,
               juce::String pluginFormat, juce::Typeface::Ptr brandTf)
            : owner (&ownerBadge), config (std::move (cfg)),
              releasesPage (ownerBadge.checker.releasesPageUrl()), brandTypeface (std::move (brandTf))
        {
            UpdateChecker& chk = ownerBadge.checker;   // construction-time only — never stored
            const juce::String mono = juce::Font::getDefaultMonospacedFontName();
            const juce::String mid  = juce::String::fromUTF8 (" \xc2\xb7 ");   // " · "
            const bool hasCore = config.coreVersion.isNotEmpty();

            // The brand byline link (under the title mark).
            link.setFont (juce::FontOptions (kTextH), false, juce::Justification::centredLeft);
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
                b.setFont (juce::FontOptions (kTextH).withName (mono), false, juce::Justification::centredLeft);
                b.setColour (juce::HyperlinkButton::textColourId, config.accentHover);
                b.changeWidthToFitText();
                addAndMakeVisible (b);
            };
            const juce::String repoBase = "https://github.com/" + chk.ownerRepo();
            const juce::String ver = displayVersionTag (chk.currentVersion());
            ghLink (verLink,    ver,                                 repoBase + "/releases/tag/" + releaseTagForCurrentVersion (chk.currentVersion()));
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
                l.setFont (juce::FontOptions (kTextH).withName (mono));
                l.setJustificationType (juce::Justification::centredLeft);
                l.setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa4));
                addAndMakeVisible (l);
            };
            juce::String tailAtxt;                                   // annotates the version line
            if (config.buildCount > 0) tailAtxt << mid << config.buildCount << " commits";
            if (config.gitDirty)       tailAtxt << mid << "dirty";
            info (tailA, tailAtxt);
            info (tailB, juce::String ("  build ") + prettyBuildStamp (config.buildNumber));   // annotates the commit line
            info (line3, pluginFormat + mid + config.os + " " + config.arch + mid + config.builder);
            if (hasCore)
            {
                info (coreLead, config.coreLabel);
                info (coreTail, config.coreVersion.fromFirstOccurrenceOf (" ", true, false));   // " (local)" or ""
            }

            check.setButtonText ("Check for updates");
            check.onClick = [this] { runCheck(); };
            addAndMakeVisible (check);

            result.setFont (juce::FontOptions (kTextH));
            result.setJustificationType (juce::Justification::centredLeft);
            result.setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa4));
            addAndMakeVisible (result);

            download.setFont (juce::FontOptions (kTextH), false, juce::Justification::centredLeft);
            download.setButtonText ("Download");
            download.setColour (juce::HyperlinkButton::textColourId, config.accentB);
            addChildComponent (download);   // hidden until an update is actually available (then setURL + setVisible)

            if (config.feedUrl.isNotEmpty())
            {
                if (config.feedPrompt.isNotEmpty())
                {
                    feedPrompt.setText (config.feedPrompt, juce::dontSendNotification);
                    feedPrompt.setFont (juce::FontOptions (kTextH));
                    feedPrompt.setJustificationType (juce::Justification::centredLeft);
                    feedPrompt.setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa4));
                    addAndMakeVisible (feedPrompt);
                }
                const juce::URL feedLink = brand::feedTheCatLink (config.productName, config.feedUrl);
                feed.setButtonText (config.feedLabel);
                feed.setURL (feedLink);
                feed.setFont (juce::FontOptions (kTextH, juce::Font::bold), false, juce::Justification::centredLeft);
                feed.setColour (juce::HyperlinkButton::textColourId, config.accentB);
                feed.setTooltip (feedLink.toString (true));
                feed.changeWidthToFitText();
                addAndMakeVisible (feed);
            }

            // What the stamp block copies: the four rows it SHOWS, in reading order. A build stamp is
            // most useful in a bug report, and a report is pasted, not retyped.
            juce::StringArray stampLines;
            stampLines.add (config.productName + " " + ver + tailAtxt);
            stampLines.add (juce::String ("g") + config.gitHash + "  build " + prettyBuildStamp (config.buildNumber));
            stampLines.add (line3.getText());
            if (hasCore)
                stampLines.add (config.coreLabel + config.coreVersion);
            stampText = stampLines.joinIntoString ("\n");

            note.setText ("Opt-in. Sends only product + version.", juce::dontSendNotification);
            note.setFont (juce::FontOptions (kTextH));
            note.setColour (juce::Label::textColourId, juce::Colour (0xff60606a));
            addAndMakeVisible (note);

            // If an update is already known from a previous check, show it up front.
            if (chk.updateAvailable())
                showUpdate (chk.storedLatest(), juce::URL (releasesPage));

            const int feedRows = config.feedUrl.isNotEmpty()            // the tip-jar block, when configured:
                                   ? 20 + (config.feedPrompt.isNotEmpty() ? kRowH : 0)   // paw row + prompt line
                                   : 0;
            setSize (kWidth, (hasCore ? 304 : 286) + feedRows + kFooterH);   // one row less without the dependency line
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

            if (! bylineArea.isEmpty() && config.drawByline != nullptr)   // the family mark, under the product's
                config.drawByline (g, bylineArea.toFloat().getCentreX(), bylineArea.toFloat().getCentreY(),
                                   (float) bylineArea.getHeight());

            if (! pawArea.isEmpty())   // the "Feed the cat" row's paw print, matching its link colour
                brand::drawPaw (g, pawArea.toFloat().getCentreX(), pawArea.toFloat().getCentreY(),
                                pawArea.toFloat().getHeight(), config.accentB);

            // The copy affordance's small print, bottom-right: the invitation, then the receipt.
            g.setFont (juce::FontOptions (kTextH).withName (juce::Font::getDefaultMonospacedFontName()));
            g.setColour (copied ? config.accentB : juce::Colour (0xff60606a));
            g.drawText (copied ? juce::String::fromUTF8 ("copied \xe2\x9c\x93") : juce::String ("click the stamp to copy"),
                        getLocalBounds().reduced (14, 12).removeFromBottom (kFooterH),
                        juce::Justification::centredRight, false);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14, 12);
            r.removeFromBottom (kFooterH);              // the copy hint's row — drawn in paint()
            titleArea = r.removeFromTop (kTitleH);      // [mark] <productName> — drawn in paint()
            auto rowB = r.removeFromTop (kBylineH);     // [family mark] by <maker>
            if (config.drawByline != nullptr)
            {
                bylineArea = rowB.removeFromLeft (kBylineH).reduced (1);
                rowB.removeFromLeft (6);
            }
            link.setBounds (rowB);
            r.removeFromTop (5);

            // Info rows: a GitHub link (fitted width) + a trailing plain label. Their union is the
            // copyable stamp block: a click anywhere in it that a hyperlink didn't take copies.
            const int stampTop = r.getY();
            auto rowV = r.removeFromTop (kRowH);
            verLink.setBounds    (rowV.removeFromLeft (verLink.getWidth()));
            tailA.setBounds      (rowV);
            auto rowC = r.removeFromTop (kRowH);
            commitLink.setBounds (rowC.removeFromLeft (commitLink.getWidth()));
            tailB.setBounds      (rowC);
            line3.setBounds      (r.removeFromTop (kRowH));
            if (config.coreVersion.isNotEmpty())
            {
                auto rowK = r.removeFromTop (kRowH);
                coreLead.setBounds (rowK.removeFromLeft ((int) textWidth (coreLead.getFont(), coreLead.getText()) + 3));
                coreLink.setBounds (rowK.removeFromLeft (coreLink.getWidth()));
                coreTail.setBounds (rowK);
            }
            stampArea = juce::Rectangle<int> (0, stampTop, getWidth(), r.getY() - stampTop);

            r.removeFromTop (7);
            check.setBounds    (r.removeFromTop (26));
            // The telemetry note hugs the button it describes; the dynamic result
            // and Download rows land below the small print, and the feed block at
            // the popup's foot keeps the whitespace between the two stories.
            r.removeFromTop (2);
            note.setBounds     (r.removeFromTop (kRowH));
            r.removeFromTop (2);
            result.setBounds   (r.removeFromTop (20));
            download.setBounds (r.removeFromTop (kRowH));
            if (feed.isVisible())
            {
                auto rowF = r.removeFromBottom (20);
                pawArea = rowF.removeFromLeft (16).reduced (0, 2);
                feed.setBounds (rowF.withTrimmedLeft (4).removeFromLeft (feed.getWidth()));
                if (feedPrompt.isVisible())
                    feedPrompt.setBounds (r.removeFromBottom (kRowH));
            }
        }

        // A click that landed on the stamp block (the links and buttons take their own first).
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! stampArea.contains (e.getPosition()))
                return;
            juce::SystemClipboard::copyTextToClipboard (stampText);
            copied = true;
            repaint();
            startTimer (1100);   // a brief receipt, then back to the invitation
        }

        void timerCallback() override { copied = false; repaint(); stopTimer(); }

        void runCheck()
        {
            // The checker is reachable ONLY through a live badge (see the struct comment): if the
            // badge — and with it, possibly, the checker — is gone, the button goes dead instead of
            // calling through a dangling reference.
            auto* live = owner.getComponent();
            if (live == nullptr)
            {
                check.setEnabled (false);
                return;
            }

            check.setEnabled (false);
            result.setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa4));
            result.setText (juce::String::fromUTF8 ("Checking\xe2\x80\xa6"), juce::dontSendNotification);
            download.setVisible (false);

            juce::Component::SafePointer<Panel> safe (this);
            live->checker.checkNow ([safe] (UpdateChecker::Result res)
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
                showUpdate (res.latest, juce::URL (res.url.isNotEmpty() ? res.url : releasesPage));
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

        juce::Component::SafePointer<VersionBadge> owner;   // the badge may die under the open popup — ALSO the only route to the checker
        const Config          config;                       // OWN copy — safe if the badge dies while the popup is open
        const juce::String    releasesPage;                 // checker.releasesPageUrl(), copied likewise (immutable derivation of the slug)
        juce::Typeface::Ptr   brandTypeface;                // the brand face for the title (from the editor; bold fallback if null)
        // The popover's type scale. The rows used to sit 4-6 px under the update button's own LnF font
        // (~15 px from its 26 px cell) — one window, two type sizes. kMonoH is the stamp's face; kRowH
        // the row that carries it; kWidth widened with the type so the environment row still fits.
        static constexpr float kTextH   = 13.0f;             // EVERY row of text; only the marks differ
        static constexpr int  kRowH     = 18;
        static constexpr int  kTitleH   = 56;                // the product mark + wordmark, twice the old
        static constexpr int  kBylineH  = 26;                // the family mark + "by <maker>"
        static constexpr int  kWidth    = 340;
        static constexpr int  kFooterH  = 18;                // the copy hint's row at the panel's foot
        juce::String          stampText;                    // what a click on the block puts on the clipboard
        juce::Rectangle<int>  stampArea;                    // the copyable rows (set by resized)
        bool                  copied = false;               // showing the receipt rather than the invitation
        juce::Rectangle<int>  titleArea;                    // where paint() draws [mark] <productName>
        juce::Rectangle<int>  bylineArea;                   // where paint() draws the family mark (if any)
        juce::Rectangle<int>  pawArea;                      // where paint() draws the feed row's paw print
        juce::Label           result, note, tailA, tailB, line3, coreLead, coreTail, feedPrompt;
        juce::HyperlinkButton link, download, verLink, commitLink, coreLink, feed;
        juce::TextButton      check;
    };

    UpdateChecker&      checker;
    Config              config;
    juce::String        format;          // running plugin format (VST3 / AU / CLAP / Standalone)
    juce::Typeface::Ptr brandTypeface;   // brand face for the popover title (set by the editor)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VersionBadge)
};

inline void VersionBadge::showPopup()
{
    launchCallOut (*this, std::make_unique<Panel> (*this, config, format, brandTypeface));
}

} // namespace felitronics::appkit

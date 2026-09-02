// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <felitronics/appkit/Brand.h>           // brand::drawOrbit — the family-default title mark
#include <felitronics/appkit/CallOut.h>         // launchCallOut — editor-parented CallOutBox
#include <felitronics/appkit/UpdateChecker.h>   // the opt-in GitHub-release check this badge fronts

#include <cmath>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

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

        // The licence the product ships under, e.g. "AGPL-3.0-or-later". Shown on the version row,
        // where a reader looking for it will look. Empty hides it.
        juce::String licence;

        // The dependency rows under the stamp: label + version, aligned into two monospaced columns
        // (a list, not a sentence), each version linking to that repo's release tag when a slug is
        // given. The legacy single-core trio below folds in as the FIRST row, so a product that only
        // ever set coreVersion keeps exactly the window it had.
        struct Dependency
        {
            juce::String label;       // "felitronics-core"
            juce::String version;     // "v0.24.0 (local)" / "8.0.14" — shown whole, suffix included
            juce::String ownerRepo;   // "darwinscat/felitronics-core"; empty => plain text, no link
        };
        std::vector<Dependency> dependencies;

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

        // An optional SECOND mark, drawn on the title row LEFT of the product's own and at the same
        // size: the maker's sign beside the product's, the way a window header carries both. Null
        // (the default) leaves the title as mark + wordmark.
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
            const juce::Font   face { juce::FontOptions (kTextH).withName (mono) };
            charW = juce::jmax (1.0f, textWidth (face, "0"));

            // ---- the table -------------------------------------------------------------------
            // Row 0 is the product; the rest is what it was built on. Every dev fact gets its own
            // COLUMN — component | version | state | commit | built — instead of being smuggled into
            // the version as a "-dirty" / "-77-gdeadbee" / " (local)" tail. Nothing is dropped; it
            // just stops being clutter.
            std::vector<Config::Dependency> data;
            data.push_back ({ config.productName, chk.currentVersion(), chk.ownerRepo() });
            if (config.coreVersion.isNotEmpty())      // the legacy single-core trio leads the deps
                data.push_back ({ config.coreLabel.trimEnd(), config.coreVersion, config.coreOwnerRepo });
            for (const auto& d : config.dependencies)
                data.push_back (d);

            std::vector<Cells> cells;
            for (size_t i = 0; i < data.size(); ++i)
            {
                auto c = splitStamp (data[i].version);
                if (i == 0)   // the product knows its own hash, dirt and build clock exactly
                {
                    c.version = releaseTagForCurrentVersion (chk.currentVersion());   // the tag it links to
                    if (config.gitDirty)              c.state  = "dirty";
                    if (config.gitHash.isNotEmpty())  c.commit = "g" + config.gitHash;
                    c.built = prettyBuildStamp (config.buildNumber);
                }
                cells.push_back (c);
            }

            // Column widths, in characters: monospace makes space-padding exact alignment.
            int wLabel = 0, wVer = 0, wState = 0, wCommit = 0;
            for (size_t i = 0; i < data.size(); ++i)
            {
                wLabel  = juce::jmax (wLabel,  data[i].label.length());
                wVer    = juce::jmax (wVer,    cells[i].version.length());
                wState  = juce::jmax (wState,  cells[i].state.length());
                wCommit = juce::jmax (wCommit, cells[i].commit.length());
            }

            auto info = [&] (juce::Label& l, const juce::String& text, juce::Colour colour)
            {
                l.setText (text, juce::dontSendNotification);
                l.setFont (juce::FontOptions (kTextH).withName (mono));
                l.setJustificationType (juce::Justification::centredLeft);
                l.setColour (juce::Label::textColourId, colour);
                addAndMakeVisible (l);
            };
            auto ghLink = [&] (juce::HyperlinkButton& b, const juce::String& text, const juce::String& url)
            {
                b.setButtonText (text);
                b.setURL (juce::URL (url));
                b.setFont (juce::FontOptions (kTextH).withName (mono), false, juce::Justification::centredLeft);
                b.setColour (juce::HyperlinkButton::textColourId, config.accentHover);
                b.changeWidthToFitText();
                addAndMakeVisible (b);
            };

            const juce::Colour ink    { 0xff9a9aa4 };   // the table's plain text
            const juce::Colour inkDim { 0xff6f6f79 };   // state / commit / built: present, not loud

            juce::StringArray stampLines;
            for (size_t i = 0; i < data.size(); ++i)
            {
                auto* row = rows.add (new Row());
                const auto& d = data[i];
                const auto& c = cells[i];

                info (row->lead, d.label.paddedRight (' ', wLabel + 1), i == 0 ? config.text : ink);
                if (d.ownerRepo.isNotEmpty() && c.version.isNotEmpty())
                    ghLink (row->link, c.version, "https://github.com/" + d.ownerRepo + "/releases/tag/" + c.version);
                else
                    info (row->plain, c.version, ink);

                // The tail completes the version column with spaces, then carries the rest, so every
                // column lines up whatever a row happens to hold.
                juce::String tail = juce::String::repeatedString (" ", juce::jmax (0, wVer - c.version.length()) + 2);
                tail << c.state .paddedRight (' ', wState  + (wState  > 0 ? 2 : 0))
                     << c.commit.paddedRight (' ', wCommit + (wCommit > 0 ? 2 : 0))
                     << c.built;
                info (row->tail, tail.trimEnd(), inkDim);

                stampLines.add ((d.label.paddedRight (' ', wLabel + 1) + c.version + tail).trimEnd());
            }

            // ---- the rows the table does not carry -------------------------------------------
            info (licence, config.licence, ink);
            info (env, pluginFormat + mid + config.os + " " + config.arch
                       + (config.builder.isNotEmpty() ? mid + config.builder : juce::String()), ink);
            stampLines.add (licence.getText());
            stampLines.add (env.getText());
            stampText = stampLines.joinIntoString ("\n");

            // The maker's byline, right of the marks.
            link.setFont (juce::FontOptions (kTextH), false, juce::Justification::centredLeft);
            link.setButtonText (config.byline);
            link.setURL (juce::URL (config.productUrl));
            link.setColour (juce::HyperlinkButton::textColourId, config.accentHover);
            link.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (link);

            check.setButtonText ("Check for updates");
            check.onClick = [this] { runCheck(); };
            addAndMakeVisible (check);

            // The consent switch for the automatic path, beside the button it automates. Off unless
            // the user turned it on; the checker persists it (see UpdateChecker::setAutoCheckEnabled).
            autoCheck.setButtonText ("Automatically, once a day");
            autoCheck.setToggleState (chk.autoCheckEnabled(), juce::dontSendNotification);
            autoCheck.setColour (juce::ToggleButton::textColourId, ink);
            autoCheck.setColour (juce::ToggleButton::tickColourId, config.accentHover);
            autoCheck.setColour (juce::ToggleButton::tickDisabledColourId, ink.withAlpha (0.5f));
            autoCheck.onClick = [this]
            {
                if (auto* live = owner.getComponent())
                    live->checker.setAutoCheckEnabled (autoCheck.getToggleState());
            };
            addAndMakeVisible (autoCheck);

            result.setFont (juce::FontOptions (kTextH));
            result.setJustificationType (juce::Justification::centredLeft);
            result.setColour (juce::Label::textColourId, ink);
            addAndMakeVisible (result);

            download.setFont (juce::FontOptions (kTextH), false, juce::Justification::centredLeft);
            download.setButtonText ("Download");
            download.setColour (juce::HyperlinkButton::textColourId, config.accentB);
            addChildComponent (download);   // hidden until an update is actually available

            if (config.feedUrl.isNotEmpty())
            {
                if (config.feedPrompt.isNotEmpty())
                {
                    feedPrompt.setText (config.feedPrompt, juce::dontSendNotification);
                    feedPrompt.setFont (juce::FontOptions (kTextH));
                    feedPrompt.setJustificationType (juce::Justification::centredLeft);
                    feedPrompt.setColour (juce::Label::textColourId, ink);
                    addAndMakeVisible (feedPrompt);
                }
                const juce::URL feedLink = brand::feedTheCatLink (config.productName, config.feedUrl);
                feed.setButtonText (config.feedLabel);
                feed.setURL (feedLink);
                feed.setFont (juce::FontOptions (kFeedH, juce::Font::bold), false, juce::Justification::centredLeft);
                feed.setColour (juce::HyperlinkButton::textColourId, config.accentB);
                feed.setTooltip (feedLink.toString (true));
                feed.changeWidthToFitText();
                addAndMakeVisible (feed);
            }

            note.setText ("Opt-in. Sends only product + version.", juce::dontSendNotification);
            note.setFont (juce::FontOptions (kTextH));
            note.setColour (juce::Label::textColourId, juce::Colour (0xff60606a));
            addAndMakeVisible (note);

            if (chk.updateAvailable())   // a check from a previous session already found one
                showUpdate (chk.storedLatest(), juce::URL (releasesPage));

            startTimerHz (24);   // the tip jar's glow (and the copy receipt's countdown)

            const int feedRows = config.feedUrl.isNotEmpty()
                                   ? kFeedRowH + (config.feedPrompt.isNotEmpty() ? kRowH : 0)
                                   : 0;
            setSize (kWidth, 268 + (int) rows.size() * kRowH + feedRows);
        }

        // "v0.24.0 (local)" / "v0.11.3-4-g6b06cba" / "8.0.14" -> the columns they really are.
        struct Cells { juce::String version, state, commit, built; };
        static Cells splitStamp (juce::String raw)
        {
            Cells c;
            raw = raw.trim();
            const juce::String head = raw.upToFirstOccurrenceOf (" ", false, false);   // drop " (local)"
            c.version = head.upToFirstOccurrenceOf ("-", false, false);
            if (raw.containsIgnoreCase ("(local)"))  c.state = "local";
            if (head.endsWithIgnoreCase ("-dirty"))  c.state = c.state.isEmpty() ? "dirty" : "local+dirty";
            const int g = head.lastIndexOf ("-g");                                      // "-77-gdeadbee"
            if (g > 0) c.commit = head.substring (g + 1).upToFirstOccurrenceOf ("-", false, false);
            return c;
        }

        void paint (juce::Graphics& g) override
        {
            // The title: the product's mark, its wordmark, then the maker's mark — SAME size, so
            // neither reads as a footnote to the other — and the byline link beside it.
            const float d  = (float) markArea.getHeight();
            const float cy = (float) titleArea.getCentreY();
            if (config.drawMark != nullptr) config.drawMark (g, markArea.toFloat().getCentreX(), cy, d);
            else                            brand::drawOrbit (g, markArea.toFloat().getCentreX(), cy, d);

            const auto wf = wordmarkFont();
            g.setFont (wf);
            g.setColour (config.text);
            g.drawSingleLineText (config.productName, wordmarkX,
                                  juce::roundToInt (cy + (wf.getAscent() - wf.getDescent()) * 0.5f));

            if (! catArea.isEmpty() && config.drawByline != nullptr)
                config.drawByline (g, catArea.toFloat().getCentreX(), cy, d);

            // The table reads as a table — and the box IS the click target that copies it.
            const auto box = stampArea.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (config.accent.withAlpha (0.28f));
            g.drawRoundedRectangle (box, 5.0f, 1.0f);

            if (! pawArea.isEmpty())
            {
                // The tip jar breathes — a valve warming, not a notification blinking: the halo and
                // the print ride the same slow sine, and neither ever goes dark.
                const float k = glow();
                const auto  p = pawArea.toFloat();
                g.setColour (config.accentB.withAlpha (0.06f + 0.10f * k));
                g.fillEllipse (p.expanded (p.getHeight() * 0.55f));
                g.setColour (config.accentB.withAlpha (0.10f + 0.14f * k));
                g.fillEllipse (p.expanded (p.getHeight() * 0.22f));
                brand::drawPaw (g, p.getCentreX(), p.getCentreY(), p.getHeight(),
                                config.accentB.withMultipliedBrightness (0.86f + 0.14f * k));
            }

            // The copy affordance's small print, bottom-right: the invitation, then the receipt.
            g.setFont (juce::FontOptions (kTextH).withName (juce::Font::getDefaultMonospacedFontName()));
            g.setColour (copied ? config.accentB : juce::Colour (0xff60606a));
            g.drawText (copied ? juce::String::fromUTF8 ("copied \xe2\x9c\x93") : juce::String ("click the table to copy it"),
                        getLocalBounds().reduced (14, 12).removeFromBottom (kFooterH),
                        juce::Justification::centredRight, false);
        }

        juce::Font wordmarkFont() const
        {
            const float h = kTitleH * 0.46f;
            return brandTypeface != nullptr
                     ? juce::Font (juce::FontOptions().withHeight (h).withTypeface (brandTypeface))
                     : juce::Font (juce::FontOptions (h, juce::Font::bold));
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14, 12);
            r.removeFromBottom (kFooterH);              // the copy hint's row — drawn in paint()

            titleArea = r.removeFromTop (kTitleH);
            {
                auto row = titleArea;
                const int d = (int) (kTitleH * 0.92f);
                markArea = row.removeFromLeft (d).withSizeKeepingCentre (d, d);
                row.removeFromLeft (8);
                wordmarkX = row.getX();
                row.removeFromLeft ((int) textWidth (wordmarkFont(), config.productName) + 16);
                if (config.drawByline != nullptr)
                {
                    catArea = row.removeFromLeft (d).withSizeKeepingCentre (d, d);
                    row.removeFromLeft (8);
                }
                link.setBounds (row.withSizeKeepingCentre (row.getWidth(), kRowH));
            }
            r.removeFromTop (8);

            // The table, inside its box.
            const int stampTop = r.getY();
            r.removeFromTop (5);
            for (auto* row : rows)
            {
                auto k = r.removeFromTop (kRowH).withTrimmedLeft (6);
                k.removeFromLeft (juce::roundToInt (charW * (float) row->lead.getText().length()));
                row->lead.setBounds (k.withX (r.getX() + 6).withWidth (k.getX() - r.getX() - 6));
                auto& versionComp = row->link.getParentComponent() != nullptr
                                      ? static_cast<juce::Component&> (row->link)
                                      : static_cast<juce::Component&> (row->plain);
                const int vw = row->link.getParentComponent() != nullptr
                                 ? row->link.getWidth()
                                 : juce::roundToInt (charW * (float) row->plain.getText().length());
                versionComp.setBounds (k.removeFromLeft (vw));
                row->tail.setBounds (k);
            }
            r.removeFromTop (5);
            stampArea = juce::Rectangle<int> (8, stampTop, getWidth() - 16, r.getY() - stampTop);

            r.removeFromTop (6);
            licence.setBounds (r.removeFromTop (kRowH));
            env.setBounds     (r.removeFromTop (kRowH));

            r.removeFromTop (8);
            {
                auto row = r.removeFromTop (26);
                check.setBounds (row.removeFromLeft (juce::jmin (220, row.getWidth())));
                row.removeFromLeft (14);
                autoCheck.setBounds (row);
            }
            // The telemetry note hugs the button it describes; the dynamic result and Download rows
            // land below the small print, and the feed block at the foot keeps the two stories apart.
            r.removeFromTop (2);
            note.setBounds     (r.removeFromTop (kRowH));
            r.removeFromTop (2);
            result.setBounds   (r.removeFromTop (20));
            download.setBounds (r.removeFromTop (kRowH));
            if (feed.isVisible())
            {
                auto rowF = r.removeFromBottom (kFeedRowH);
                feedArea = rowF;
                pawArea = rowF.removeFromLeft (kFeedRowH).reduced (2);
                feed.setBounds (rowF.withTrimmedLeft (6).removeFromLeft (feed.getWidth()));
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
            copiedFrames = 27;   // ~1.1 s at 24 Hz, then back to the invitation
            repaint();
        }

        // 24 Hz: the tip jar's glow advances, and the copy receipt counts itself down.
        void timerCallback() override
        {
            if (copiedFrames > 0 && --copiedFrames == 0)
            {
                copied = false;
                repaint();
            }

            phase += 0.026f;   // ~2.6 s a cycle at 24 Hz — a breath, not a blink
            if (phase > juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;

            if (feed.isVisible())
            {
                feed.setColour (juce::HyperlinkButton::textColourId,
                                config.accentB.withMultipliedBrightness (0.86f + 0.14f * glow()));
                repaint (feedArea);
            }
        }

        float glow() const { return 0.5f + 0.5f * std::sin (phase); }

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

        // The popover's scale. ONE text size for every row (the update button keeps the size its
        // LookAndFeel derives from its cell — it is meant to be the loud thing); the marks scale with
        // the title row; the width is set by the TABLE, which is the window's centre of gravity.
        static constexpr float kTextH   = 13.0f;
        static constexpr float kFeedH   = 16.0f;             // the tip jar speaks a size louder
        static constexpr int  kRowH     = 18;
        static constexpr int  kFeedRowH = 26;                // the paw's row
        static constexpr int  kTitleH   = 72;                // [product mark] <wordmark> [maker mark] by <maker>
        static constexpr int  kWidth    = 600;
        static constexpr int  kFooterH  = 18;                // the copy hint's row at the panel's foot

        float                 charW = 8.0f;                 // one monospaced advance (column arithmetic)
        juce::String          stampText;                    // what a click on the table puts on the clipboard
        juce::Rectangle<int>  stampArea;                    // the table's box — also the click target
        bool                  copied = false;               // showing the receipt rather than the invitation
        int                   copiedFrames = 0;             // frames left on the receipt
        float                 phase = 0.0f;                 // the tip jar's glow
        juce::Rectangle<int>  feedArea;                     // the row the glow repaints
        juce::Rectangle<int>  titleArea, markArea, catArea; // where paint() draws the title row
        int                   wordmarkX = 0;
        juce::Rectangle<int>  pawArea;                      // where paint() draws the feed row's paw print

        // One table row: "<component>" | version (linked when the row has a repo) | the dim rest.
        struct Row
        {
            juce::Label           lead, plain, tail;
            juce::HyperlinkButton link;
        };
        juce::OwnedArray<Row> rows;

        juce::Label           result, note, licence, env, feedPrompt;
        juce::HyperlinkButton link, download, feed;
        juce::ToggleButton    autoCheck;
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

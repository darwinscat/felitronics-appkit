// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// Theory-driven falsification for the chrome layer (felitronics::appkit::chrome). Every check derives
// its expectation from the CONTRACT and tries to refute the code — never mirrors the implementation.
// Headless: render into a juce::Image and count painted pixels (no window, no peer), and drive the
// gesture handlers DIRECTLY (a live startDragging asserts without a peer).
#include <felitronics/appkit/chrome/BrandBlister.h>
#include <felitronics/appkit/chrome/ChromeBar.h>
#include <felitronics/appkit/chrome/ChromeMetrics.h>
#include <felitronics/appkit/chrome/ChromeUnderline.h>
#include <felitronics/appkit/chrome/CompareCell.h>
#include <felitronics/appkit/chrome/FlatButtons.h>
#include <felitronics/appkit/chrome/PresetCell.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace chrome = felitronics::appkit::chrome;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

// A vivid, unambiguous theme so a painted pixel's hue tells us WHICH contract drew it.
static chrome::ChromeTheme vividTheme()
{
    return { .fill       = juce::Colour (0xff000000),
             .underline  = juce::Colour (0xffffffff),
             .accent     = juce::Colour (0xffff0000),   // RED   — active-register frame
             .attention  = juce::Colour (0xff00ff00),   // GREEN — edited dot + drop ring
             .text       = juce::Colour (0xff808080),
             .textDim    = juce::Colour (0xff404040),
             .activeText = juce::Colour (0xff0000ff) };  // BLUE — on-state label (a DISTINCT hue, not white,
}                                                        // so the seam can't hide behind the old hardcoded white)

static int countIf (const juce::Image& img, bool (*pred) (juce::Colour))
{
    int n = 0;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (const auto p = img.getPixelAt (x, y); p.getAlpha() > 0 && pred (p))
                ++n;
    return n;
}

static bool isRed   (juce::Colour p) { return p.getRed()   > 150 && p.getGreen() < 90 && p.getBlue() < 90; }
static bool isGreen (juce::Colour p) { return p.getGreen() > 150 && p.getRed()   < 90 && p.getBlue() < 90; }
static bool isBlue  (juce::Colour p) { return p.getBlue()  > 150 && p.getRed()   < 90 && p.getGreen() < 90; }

static juce::Image renderButton (chrome::RegisterButton& b, bool toggled, bool highlighted, bool down)
{
    b.setToggleState (toggled, juce::dontSendNotification);
    juce::Image img (juce::Image::ARGB, b.getWidth(), b.getHeight(), true);
    juce::Graphics g (img);
    b.paintButton (g, highlighted, down);
    return img;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("felitronics::appkit chrome falsification tests\n");

    // ---- the active-register accent frame is state-gated (not always/never drawn) -----------------
    group ("the accent frame appears IFF the register is the active one");
    {
        chrome::RegisterButton b;
        b.theme = vividTheme();
        b.setButtonText ("A");   // give the label something to draw so the activeText seam is observable
        b.setSize (26, 26);

        ok (countIf (renderButton (b, false, false, false), isRed) == 0,
            "an INACTIVE register draws NO accent frame");
        ok (countIf (renderButton (b, true, false, false), isRed) > 0,
            "the ACTIVE register draws the accent frame");
        // The seam is the theme, not a hardcoded palette: recolour and the frame follows.
        b.theme.accent = juce::Colour (0xff00ff00);
        ok (countIf (renderButton (b, true, false, false), isRed) == 0
                && countIf (renderButton (b, true, false, false), isGreen) > 0,
            "the frame colour tracks theme.accent (theme seam, no hardcoded palette)");

        // The on-state LABEL uses theme.activeText (a distinct BLUE), NOT the old hardcoded white — so
        // reverting activeText to Colours::white could no longer sneak past the suite.
        b.theme = vividTheme();
        ok (countIf (renderButton (b, true, false, false), isBlue) > 0,
            "the ACTIVE register's label is painted in theme.activeText");
        ok (countIf (renderButton (b, false, false, false), isBlue) == 0,
            "an INACTIVE register's label does NOT use theme.activeText");
    }

    // ---- the edited dot is state-gated + uses theme.attention -------------------------------------
    group ("the edited dot appears IFF the register is edited");
    {
        chrome::RegisterButton b;
        b.theme = vividTheme();
        b.setSize (26, 26);
        b.setEdited (false);
        ok (countIf (renderButton (b, false, false, false), isGreen) == 0, "a clean register draws no attention dot");
        b.setEdited (true);
        ok (countIf (renderButton (b, false, false, false), isGreen) > 0, "an edited register draws the attention dot");
    }

    // ---- right-click gates to the copy menu, NEVER recalls (juce::Button clicks on ANY button) ----
    group ("a right-click fires the copy menu and never recalls the register");
    {
        chrome::RegisterButton b;
        b.theme = vividTheme();
        b.setSize (26, 26);
        bool recalled = false, popped = false;
        b.onClick = [&] { recalled = true; };
        b.onPopup = [&] { popped = true; };

        const auto src = juce::Desktop::getInstance().getMainMouseSource();
        const juce::ModifierKeys rmb (juce::ModifierKeys::rightButtonModifier);
        const juce::Point<float> pos (13.0f, 13.0f);
        const juce::MouseEvent e (src, pos, rmb,
            juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
            juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
            juce::MouseInputSource::defaultTiltY, &b, &b,
            juce::Time::getCurrentTime(), pos, juce::Time::getCurrentTime(), 1, false);
        b.mouseDown (e);
        b.mouseUp (e);
        ok (popped && ! recalled, "right-click → onPopup, and the register is NOT recalled");
    }

    // ---- the DnD payload is chrome-namespaced + a register is never interested in its own tag ------
    group ("drag-source interest matches only sibling chrome-register tags");
    {
        chrome::RegisterButton b;
        b.setRegisterIndex (2);
        ok (chrome::RegisterButton::dragTag (2).startsWith ("felitronics.appkit.chrome/register:"),
            "the drag tag is namespaced to the chrome layer");

        struct SrcOf : juce::DragAndDropTarget::SourceDetails
        {
            explicit SrcOf (const juce::String& d) : SourceDetails (d, nullptr, {}) {}
        };
        ok (b.isInterestedInDragSource (SrcOf (chrome::RegisterButton::dragTag (0))),
            "a sibling register's drag interests this one");
        ok (! b.isInterestedInDragSource (SrcOf (chrome::RegisterButton::dragTag (2))),
            "a register is NOT interested in its OWN drag");
        ok (! b.isInterestedInDragSource (SrcOf ("some.other.app/thing:0")),
            "a foreign drag tag is rejected");
    }

    // ---- setModel maps active→toggle; the register COUNT clamps to [1, kMaxRegisters] --------------
    group ("CompareCell maps the model and clamps the register count");
    {
        const auto theme = vividTheme();
        chrome::CompareCell cell (4, theme);

        auto toggledOf = [&cell] (int i)
        {
            return static_cast<chrome::RegisterButton*> (cell.registerAnchor (i))->getToggleState();
        };

        chrome::CompareModel m;
        m.active = -1;
        cell.setModel (m);
        ok (! toggledOf (0) && ! toggledOf (1) && ! toggledOf (2) && ! toggledOf (3),
            "model.active = -1 → no register is toggled (no accent frame anywhere)");

        m.active = 2;
        cell.setModel (m);
        ok (toggledOf (2) && ! toggledOf (0) && ! toggledOf (1) && ! toggledOf (3),
            "model.active = 2 → exactly register C is toggled");

        ok (chrome::CompareCell (1, theme).registerCount() == 1, "count clamps up to the floor of 1");
        ok (chrome::CompareCell (8, theme).registerCount() == 8, "count passes through at the cap of 8");
        ok (chrome::CompareCell (9, theme).registerCount() == chrome::CompareModel::kMaxRegisters,
            "count clamps down to kMaxRegisters (= 8)");
    }

    // ---- BrandBlister::preferredWidth: the FRAME owns skirts+pads, the mark reports CONTENT only ---
    group ("the blister wraps the mark's content width in its own skirt + pad geometry (C6)");
    {
        const chrome::ChromeMetrics metrics;
        const auto theme = vividTheme();
        chrome::BrandBlister blister (metrics, theme);

        // Contract LITERALS (not the header's constants, so a formula slip can't mirror into the oracle):
        // endFlat 12 + transW 30, doubled = 84 minimum; + padL 5 + padR 5 = 94 skirts/pads; + 100 content = 194.
        ok (blister.preferredWidth (46) == 84,
            "an EMPTY mark yields the contract minimum 84 (room for two 42px transitions), never a degenerate 0");

        struct StubMark : chrome::BlisterMark
        {
            int w;
            explicit StubMark (int cw) : w (cw) {}
            int preferredContentWidth (int) const override { return w; }
        };
        StubMark mark (100);
        blister.setMark (&mark);
        ok (blister.preferredWidth (46) == 194,
            "preferredWidth = 94 skirts/pads + 100 content = 194 — the frame passes the int content through, adding no rounding of its own");
        ok (blister.preferredWidth (46) > mark.preferredContentWidth (46),
            "the frame ADDS to the content width (skirts are not free)");

        // C7-symmetry: the frame is foolproof against its (non-owned) mark being destroyed FIRST —
        // it self-subscribes as a ComponentListener and nulls the pointer, exactly like the underline.
        chrome::BrandBlister hardened (metrics, theme);
        auto* heapMark = new StubMark (100);
        hardened.setMark (heapMark);
        ok (hardened.preferredWidth (46) == 194, "with a live mark the frame reports the full width");
        delete heapMark;   // destroy the mark out from under the frame (mis-ordered teardown)
        ok (hardened.preferredWidth (46) == 84,
            "after its mark is destroyed the frame falls back to the empty-mark minimum (no dangling read)");
        hardened.setSize (120, 46);
        juce::Image himg (juce::Image::ARGB, 120, 46, true);
        juce::Graphics hg (himg);
        hardened.paint (hg);   // must not crash / deref the dead mark
        ok (true, "the frame paints safely after its mark is destroyed");
    }

    // ---- ChromeBar clamps the even gap at 0 so a narrow window never overlaps cells ---------------
    group ("a narrow band clamps the gap at 0 — cells butt, never overlap");
    {
        std::vector<std::unique_ptr<juce::Component>> dummies;
        chrome::ChromeBar bar;
        for (int i = 0; i < 5; ++i)
        {
            dummies.push_back (std::make_unique<juce::Component>());
            chrome::Cell c;
            c.component  = dummies.back().get();
            c.region     = chrome::Cell::Region::RigidCenter;
            c.fixedWidth = 40;
            bar.add (c);
        }
        bar.layout ({ 0, 0, 60, 30 }, 30, 0);   // avail 60 ≪ 5×40 fixed → the raw gap would go negative

        bool noOverlap = true;
        int prevRight = -1000000;
        for (auto& d : dummies)
        {
            noOverlap = noOverlap && d->getX() >= prevRight;
            prevRight = d->getRight();
        }
        ok (noOverlap, "with the gap clamped at 0, no cell starts left of the previous cell's right edge");
    }

    // ---- ChromeUnderline self-subscribes AND survives its blister being destroyed first (C7) -------
    group ("the underline is foolproof if the blister is torn down first");
    {
        const chrome::ChromeMetrics metrics;
        const auto theme = vividTheme();
        auto blister = std::make_unique<chrome::BrandBlister> (metrics, theme);
        chrome::ChromeUnderline underline (*blister, metrics, theme);
        underline.setSize (200, 46);

        blister.reset();   // destroy the blister while the overlay is still alive (mis-ordered teardown)

        juce::Image img (juce::Image::ARGB, 200, 46, true);
        juce::Graphics g (img);
        underline.paint (g);   // must be a safe no-op now — no dangling read, nothing painted
        int painted = 0;
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
                if (img.getPixelAt (x, y).getAlpha() > 0) ++painted;
        ok (painted == 0, "after the blister is destroyed, the underline paints nothing (no dangling reference)");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

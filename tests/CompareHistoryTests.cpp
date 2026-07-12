// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// Theory unit for CompareHistory — the shared undo/redo + A/B/C/D engine. Expectations come from the
// design CONTRACT, not the implementation: the settle-timer state machine (a burst = one step), the
// undo/redo stack discipline (redo cleared on a fresh commit, undo bounded by maxUndo), the "active
// register = live, never stored twice" invariant, the persistence envelope shape, and the two
// topologies (WholeWorkspace = a switch is on the undo timeline; PerRegister = each register its own
// history, a switch is not; recall/stamp are discrete edits committed immediately).

#include <felitronics/appkit/CompareHistory.h>

#include <cstdio>

using CH = felitronics::appkit::CompareHistory;

static int failures = 0;
static void check (bool ok, const char* what)
{
    if (! ok) { std::printf ("FAIL: %s\n", what); ++failures; }
    else        std::printf ("ok:   %s\n", what);
}

//== a minimal opaque "live state": one integer carried in a byte-stable <S v=..> tree ============
struct MockConsumer
{
    int live = 0;
    juce::ValueTree capture() const { juce::ValueTree t ("S"); t.setProperty ("v", live, nullptr); return t; }
    void apply (const juce::ValueTree& t) { live = static_cast<int> (t.getProperty ("v", 0)); }
};

static CH make (MockConsumer& c, CH::Mode mode, CH::Config cfg)
{
    return CH (mode,
               [&c] { return c.capture(); },
               [&c] (const juce::ValueTree& t) { c.apply (t); },
               cfg);
}

// Settle a just-made edit into one committed undo step: 1 tick to register the change + settleTicks
// stable ticks to cross the threshold (+slack).
static void settle (CH& h, int settleTicks) { for (int i = 0; i < settleTicks + 3; ++i) h.tick(); }

int main()
{
    constexpr int kSettle = 3;
    auto cfgN = [] (int n) { CH::Config c; c.numRegisters = n; c.settleTicks = kSettle; return c; };

    //== WholeWorkspace (OrbitCab topology) ======================================================
    {
        auto cfg = cfgN (4); cfg.maxUndo = 8;
        MockConsumer c; c.live = 10;
        auto h = make (c, CH::Mode::WholeWorkspace, cfg);
        h.tick();

        check (! h.canUndo() && ! h.canRedo(), "WW: fresh history has nothing to undo/redo");
        c.live = 20; settle (h, kSettle);
        check (h.canUndo(), "WW: a settled edit is undoable");
        h.undo();
        check (c.live == 10, "WW: undo restores the pre-edit live state");
        check (h.canRedo(), "WW: undo enables redo");
        h.redo();
        check (c.live == 20, "WW: redo re-applies the edit");

        MockConsumer c2; c2.live = 0; auto h2 = make (c2, CH::Mode::WholeWorkspace, cfg); h2.tick();
        for (int v = 1; v <= 5; ++v) { c2.live = v; h2.tick(); }
        settle (h2, kSettle);
        h2.undo();
        check (c2.live == 0, "WW: a burst of edits coalesces into ONE undo step");
        check (! h2.canUndo(), "WW: the burst was a single step (stack now empty)");

        MockConsumer c3; c3.live = 0; auto h3 = make (c3, CH::Mode::WholeWorkspace, cfg); h3.tick();
        c3.live = 1; settle (h3, kSettle); h3.undo();
        check (h3.canRedo(), "WW: redo available after undo");
        c3.live = 9; settle (h3, kSettle);
        check (! h3.canRedo(), "WW: a fresh commit clears the redo stack");
    }

    //== WholeWorkspace: a register switch IS on the undo timeline ================================
    {
        MockConsumer c; c.live = 10; auto h = make (c, CH::Mode::WholeWorkspace, cfgN (4)); h.tick();
        h.switchTo (1);
        check (h.active() == 1, "WW: switchTo moves the active register");
        settle (h, kSettle);
        check (h.canUndo(), "WW: a register switch is recorded on the undo timeline");
        h.undo();
        check (h.active() == 0, "WW: undo walks the register switch back to A");
    }

    //== maxUndo trim: the stack never exceeds the cap (oldest dropped) ===========================
    {
        auto cfg = cfgN (4); cfg.maxUndo = 3;
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::WholeWorkspace, cfg); h.tick();
        for (int v = 1; v <= 6; ++v) { c.live = v; settle (h, kSettle); }   // 6 distinct committed edits
        int depth = 0;
        while (h.canUndo()) { h.undo(); ++depth; }
        check (depth == 3, "maxUndo caps the undo stack at 3 (oldest edits dropped)");
    }

    //== persistence envelope: shape + round-trip + active-register invariant (WholeWorkspace) ====
    {
        MockConsumer c; c.live = 7; auto h = make (c, CH::Mode::WholeWorkspace, cfgN (4)); h.tick();
        h.switchTo (1); c.live = 99; h.switchTo (0);
        const juce::ValueTree tree = h.toTree();

        check (tree.hasType ("Workspace"), "envelope root is <Workspace>");
        check (static_cast<int> (tree.getProperty ("active", -1)) == 0, "envelope records active index");
        const auto snaps = tree.getChildWithName ("Snaps");
        check (snaps.isValid() && snaps.getNumChildren() == 4, "envelope emits one <Snap> per register (incl. empty)");
        bool activeEmpty = true, bStored = false;
        for (const auto snap : snaps)
        {
            const int i = static_cast<int> (snap.getProperty ("i", -1));
            if (i == 0 && snap.getNumChildren() > 0) activeEmpty = false;
            if (i == 1 && snap.getNumChildren() > 0) bStored = true;
        }
        check (activeEmpty, "the active register is NOT stored in the envelope (nullopt)");
        check (bStored, "an inactive register with content IS stored");

        MockConsumer c2; auto h2 = make (c2, CH::Mode::WholeWorkspace, cfgN (4));
        h2.fromTree (tree);
        check (h2.active() == 0 && c2.live == 7, "fromTree restores active + live");
        h2.switchTo (1);
        check (c2.live == 99, "fromTree restored the stored B register");
        h2.switchTo (0);
        bool aEmpty2 = true;
        for (const auto snap : h2.toTree().getChildWithName ("Snaps"))
            if (static_cast<int> (snap.getProperty ("i", -1)) == 0 && snap.getNumChildren() > 0) aEmpty2 = false;
        check (aEmpty2, "fromTree preserved the active-is-nullopt invariant (no double-store)");
    }

    //== PerRegister (TabbyEQ topology) ==========================================================
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();

        c.live = 5; settle (h, kSettle);
        check (h.canUndo(), "PR: edit in A is undoable in A");
        h.switchTo (1);
        check (! h.canUndo(), "PR: a switch is NOT an undo step (B has no history)");
        settle (h, kSettle);
        check (! h.canUndo(), "PR: switching never records the recalled state as an edit");

        c.live = 8; settle (h, kSettle);
        check (h.canUndo(), "PR: edit in B is undoable in B");
        h.undo();
        check (c.live == 5, "PR: undo in B reverts B's edit (B's baseline was the switched-in 5)");
        h.switchTo (0);
        check (h.canUndo(), "PR: A's own history survives the excursion to B");
        h.undo();
        check (c.live == 0, "PR: undo in A reverts A's edit, independent of B");
    }

    //== PerRegister: an UNSETTLED edit is flushed on switch-away (crew: codex #1 / opus #1) ======
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 5;                                          // edit A, but do NOT settle
        h.switchTo (1);                                      // switch away mid-edit
        h.switchTo (0);                                      // come back to A
        check (h.canUndo(), "PR: a mid-edit tweak is flushed on switch-away, not lost");
        h.undo();
        check (c.live == 0, "PR: undo restores the pre-tweak state after a switch-away");
    }

    //== PerRegister: recall is committed IMMEDIATELY (crew: codex #2 / opus #2 — the recall race) ==
    {
        MockConsumer c; c.live = 1; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.switchTo (3); c.live = 40; h.switchTo (0);         // D holds 40, back on A(=1)
        h.tick();                                            // A baseline seeded at 1
        h.recallInto (3);
        h.undo();                                            // IMMEDIATELY — no settle in between
        check (c.live == 1, "PR: recall is a discrete undo step even with no tick before undo");
        h.redo();
        check (c.live == 40, "PR: redo re-applies the recall");
    }

    //== PerRegister: stamp into a NON-empty target is undoable; into an EMPTY target is its birth ==
    {
        MockConsumer c; c.live = 1; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.switchTo (3); c.live = 40; h.switchTo (0);         // D = 40 (non-empty)
        c.live = 7; h.stampInto (3);                         // overwrite D with 7
        h.switchTo (3);
        check (c.live == 7, "PR: stamp overwrote the non-empty target");
        h.undo();
        check (c.live == 40, "PR: stamp into a non-empty register is undoable (old D restored)");

        // stamp into a never-used register C: no crash, no bogus undo entry (it's C's birth)
        MockConsumer c2; c2.live = 3; auto h2 = make (c2, CH::Mode::PerRegister, cfgN (4)); h2.tick();
        h2.stampInto (2);                                    // C was never used
        h2.switchTo (2);
        check (c2.live == 3, "PR: stamp into an empty register sets its birth content");
        check (! h2.canUndo(), "PR: stamping into an empty register records NO undo entry (its origin)");
    }

    //== PerRegister persistence round-trip (opus #8 — the TabbyEQ save/load path) =================
    {
        MockConsumer c; c.live = 11; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.switchTo (2); c.live = 55; h.switchTo (0);         // C = 55, active A(=11)
        const juce::ValueTree tree = h.toTree();

        MockConsumer c2; auto h2 = make (c2, CH::Mode::PerRegister, cfgN (4));
        h2.fromTree (tree);
        check (h2.active() == 0 && c2.live == 11, "PR: fromTree restores active + live");
        check (! h2.canUndo(), "PR: a load is a fresh history boundary (no undo)");
        h2.switchTo (2);
        check (c2.live == 55, "PR: fromTree restored the stored C register in PerRegister mode");
    }

    //== onAfterApply reason cardinality (opus #8) ================================================
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4));
        int nSwitch = 0, nUndo = 0, nRedo = 0, nCopy = 0, nLoad = 0;
        h.onAfterApply = [&] (CH::Reason r)
        {
            switch (r)
            {
                case CH::Reason::Switch: ++nSwitch; break;
                case CH::Reason::Undo:   ++nUndo;   break;
                case CH::Reason::Redo:   ++nRedo;   break;
                case CH::Reason::Copy:   ++nCopy;   break;
                case CH::Reason::Load:   ++nLoad;   break;
            }
        };
        h.tick();
        h.switchTo (1);                          // Switch
        c.live = 9; settle (h, kSettle); h.undo();   // Undo
        h.redo();                                // Redo
        h.stampInto (2);                         // Copy
        h.fromTree (h.toTree());                 // Load
        check (nSwitch == 1, "onAfterApply fires exactly once for Switch");
        check (nUndo == 1 && nRedo == 1, "onAfterApply fires once each for Undo and Redo");
        check (nCopy == 1, "onAfterApply fires once for a stamp (Copy)");
        check (nLoad == 1, "onAfterApply fires once for a load");
    }

    std::printf (failures == 0 ? "CompareHistory: all checks passed\n" : "CompareHistory: %d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}

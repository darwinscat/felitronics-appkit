// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// Theory unit for CompareHistory — the shared undo/redo + A/B/C/D engine. Expectations come from the
// design CONTRACT, not the implementation: the settle-timer state machine (a burst = one step), the
// undo/redo stack discipline (redo cleared on a fresh commit), the "active register = live, never
// stored twice" invariant, the persistence envelope shape, and the two topologies (WholeWorkspace =
// a switch is on the undo timeline; PerRegister = each register its own history, a switch is not).

#include <felitronics/appkit/CompareHistory.h>

#include <cstdio>

using felitronics::appkit::CompareHistory;

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

static CompareHistory make (MockConsumer& c, CompareHistory::Config cfg)
{
    return CompareHistory ([&c] { return c.capture(); },
                           [&c] (const juce::ValueTree& t) { c.apply (t); },
                           cfg);
}

// Settle a just-made edit into one committed undo step: 1 tick to register the change + settleTicks
// stable ticks to cross the threshold (+slack).
static void settle (CompareHistory& h, int settleTicks) { for (int i = 0; i < settleTicks + 3; ++i) h.tick(); }

int main()
{
    constexpr int kSettle = 3;

    //== WholeWorkspace (OrbitCab topology) ======================================================
    {
        CompareHistory::Config cfg; cfg.mode = CompareHistory::Mode::WholeWorkspace;
        cfg.numRegisters = 4; cfg.maxUndo = 8; cfg.settleTicks = kSettle;

        MockConsumer c; c.live = 10;
        auto h = make (c, cfg);
        h.tick();                                        // seed baseline at v=10

        check (! h.canUndo() && ! h.canRedo(), "WW: fresh history has nothing to undo/redo");

        c.live = 20; settle (h, kSettle);                // one edit -> one committed step
        check (h.canUndo(), "WW: a settled edit is undoable");
        h.undo();
        check (c.live == 10, "WW: undo restores the pre-edit live state");
        check (h.canRedo(), "WW: undo enables redo");
        h.redo();
        check (c.live == 20, "WW: redo re-applies the edit");

        // Coalescing: a burst of intermediate values that settles is ONE undo step, not many.
        MockConsumer c2; c2.live = 0; auto h2 = make (c2, cfg); h2.tick();
        for (int v = 1; v <= 5; ++v) { c2.live = v; h2.tick(); }   // rapid changes, never stable
        settle (h2, kSettle);                                       // then it settles at v=5
        h2.undo();
        check (c2.live == 0, "WW: a burst of edits coalesces into ONE undo step");
        check (! h2.canUndo(), "WW: the burst was a single step (stack now empty)");

        // A fresh commit clears redo.
        MockConsumer c3; c3.live = 0; auto h3 = make (c3, cfg); h3.tick();
        c3.live = 1; settle (h3, kSettle); h3.undo();               // redo now available
        check (h3.canRedo(), "WW: redo available after undo");
        c3.live = 9; settle (h3, kSettle);                          // a new edit
        check (! h3.canRedo(), "WW: a fresh commit clears the redo stack");
    }

    //== WholeWorkspace: a register switch IS on the undo timeline ================================
    {
        CompareHistory::Config cfg; cfg.mode = CompareHistory::Mode::WholeWorkspace;
        cfg.numRegisters = 4; cfg.settleTicks = kSettle;
        MockConsumer c; c.live = 10; auto h = make (c, cfg); h.tick();

        h.switchTo(1);                                   // A->B; B empty so live stays, but active changed
        check (h.active() == 1, "WW: switchTo moves the active register");
        settle (h, kSettle);                             // the workspace change settles as a step
        check (h.canUndo(), "WW: a register switch is recorded on the undo timeline");
        h.undo();
        check (h.active() == 0, "WW: undo walks the register switch back to A");
    }

    //== persistence envelope: shape + round-trip + active-register invariant =====================
    {
        CompareHistory::Config cfg; cfg.numRegisters = 4; cfg.settleTicks = kSettle;
        MockConsumer c; c.live = 7; auto h = make (c, cfg); h.tick();

        // stash a keeper into B (switch there, set a value, that becomes B when we leave), then
        // return to A so B holds a stored snapshot and A is live/active.
        h.switchTo(1); c.live = 99; h.switchTo(0);       // A active; B stored = 99
        const juce::ValueTree tree = h.toTree();

        check (tree.hasType ("Workspace"), "envelope root is <Workspace>");
        check (static_cast<int> (tree.getProperty ("active", -1)) == 0, "envelope records active index");
        const auto snaps = tree.getChildWithName ("Snaps");
        check (snaps.isValid() && snaps.getNumChildren() == 4, "envelope emits one <Snap> per register (incl. empty)");
        // the ACTIVE register (A=0) must be empty in the envelope (live is authoritative)
        bool activeEmpty = true, bStored = false;
        for (const auto snap : snaps)
        {
            const int i = static_cast<int> (snap.getProperty ("i", -1));
            if (i == 0 && snap.getNumChildren() > 0) activeEmpty = false;
            if (i == 1 && snap.getNumChildren() > 0) bStored = true;
        }
        check (activeEmpty, "the active register is NOT stored in the envelope (nullopt)");
        check (bStored, "an inactive register with content IS stored");

        // round-trip into a fresh engine
        MockConsumer c2; auto h2 = make (c2, cfg);
        h2.fromTree (tree);
        check (h2.active() == 0 && c2.live == 7, "fromTree restores active + live");
        h2.switchTo(1);
        check (c2.live == 99, "fromTree restored the stored B register");
        // after fromTree the active register must be re-nulled: re-emit and confirm B(now active)...
        h2.switchTo(0);
        const auto snaps2 = h2.toTree().getChildWithName ("Snaps");
        bool aEmpty2 = true;
        for (const auto snap : snaps2) if (static_cast<int> (snap.getProperty ("i", -1)) == 0 && snap.getNumChildren() > 0) aEmpty2 = false;
        check (aEmpty2, "fromTree preserved the active-is-nullopt invariant (no double-store)");
    }

    //== PerRegister (TabbyEQ topology) ==========================================================
    {
        CompareHistory::Config cfg; cfg.mode = CompareHistory::Mode::PerRegister;
        cfg.numRegisters = 4; cfg.settleTicks = kSettle;
        MockConsumer c; c.live = 0; auto h = make (c, cfg); h.tick();

        // edit A, then switch to B: the switch must NOT create an undo step in B.
        c.live = 5; settle (h, kSettle);
        check (h.canUndo(), "PR: edit in A is undoable in A");
        h.switchTo(1);
        check (! h.canUndo(), "PR: a switch is NOT an undo step (B has no history)");
        settle (h, kSettle);                              // even after settling, the recalled seed is B's baseline
        check (! h.canUndo(), "PR: switching never records the recalled state as an edit");

        // edit B, undo in B affects only B; switch back to A leaves A's history intact.
        c.live = 8; settle (h, kSettle);
        check (h.canUndo(), "PR: edit in B is undoable in B");
        h.undo();
        check (c.live == 5, "PR: undo in B reverts B's edit (B's baseline was the switched-in 5)");
        h.switchTo(0);
        check (h.canUndo(), "PR: A's own history survives the excursion to B");
        h.undo();
        check (c.live == 0, "PR: undo in A reverts A's edit, independent of B");
    }

    //== PerRegister: recall = edit in current; stamp = edit in target ============================
    {
        CompareHistory::Config cfg; cfg.mode = CompareHistory::Mode::PerRegister;
        cfg.numRegisters = 4; cfg.settleTicks = kSettle;
        MockConsumer c; c.live = 1; auto h = make (c, cfg); h.tick();

        // put a keeper in D (switch there, set 40, leave) then return to A
        h.switchTo(3); c.live = 40; h.switchTo(0);
        // recall D into current A: live becomes 40, undoable in A
        c.live = 1;                                       // A is back to 1 after the round-trip seed
        h.tick();                                          // reseed A baseline at 1
        h.recallInto(3);
        settle (h, kSettle);
        check (c.live == 40, "PR: recallInto pulls the source register into live");
        h.undo();
        check (c.live == 1, "PR: recall is an undoable edit in the current register");

        // stamp current A(=1) into D: D's stored content becomes 1, undoable within D
        c.live = 1; h.stampInto(3);
        h.switchTo(3);
        check (c.live == 1, "PR: stampInto overwrote the target register");
        h.undo();
        check (c.live == 40, "PR: stamp is undoable within the target register (old D restored)");
    }

    std::printf (failures == 0 ? "CompareHistory: all checks passed\n" : "CompareHistory: %d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}

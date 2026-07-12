// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// Theory unit for CompareHistory — the shared undo/redo + A/B/C/D engine. Expectations come from the
// design CONTRACT, not the implementation: the settle-timer state machine (a burst = one step), the
// undo/redo stack discipline (redo cleared on a fresh commit, undo bounded by maxUndo), the "active
// register = live, never stored twice" invariant, the persistence envelope shape, and the two
// topologies (WholeWorkspace = a switch is on the undo timeline; PerRegister = each register its own
// history, a switch is not; copyRegister/applyEdit are discrete edits committed immediately).

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

// A byte-stable opaque payload tree, shaped like MockConsumer::capture() — for external/clipboard content.
static juce::ValueTree mk (int v) { juce::ValueTree t ("S"); t.setProperty ("v", v, nullptr); return t; }

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

    //== copyRegister INTO the current register (recall) — committed IMMEDIATELY (the recall race) ==
    {
        MockConsumer c; c.live = 1; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.switchTo (3); c.live = 40; h.switchTo (0);         // D holds 40, back on A(=1)
        h.tick();                                            // A baseline seeded at 1
        h.copyRegister (3, 0);                               // "copy here (A) from D" — D's 40 into current A
        h.undo();                                            // IMMEDIATELY — no settle in between
        check (c.live == 1, "copy-into-current is a discrete undo step even with no tick before undo");
        h.redo();
        check (c.live == 40, "redo re-applies the copy-into-current");
    }

    //== copyRegister OUT to a register (stamp): non-empty target undoable, empty target is birth ===
    {
        MockConsumer c; c.live = 1; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.switchTo (3); c.live = 40; h.switchTo (0);         // D = 40 (non-empty), active A
        c.live = 7; h.copyRegister (0, 3);                   // "copy this (A=7) to D" — overwrite D
        h.switchTo (3);
        check (c.live == 7, "copy-to-register overwrote the non-empty target");
        h.undo();
        check (c.live == 40, "copy into a non-empty register is undoable (old D restored)");

        // copy the active into a never-used register C: no crash, no bogus undo entry (C's birth)
        MockConsumer c2; c2.live = 3; auto h2 = make (c2, CH::Mode::PerRegister, cfgN (4)); h2.tick();
        h2.copyRegister (0, 2);                              // A(=3) into never-used C
        h2.switchTo (2);
        check (c2.live == 3, "copy into an empty register sets its birth content");
        check (! h2.canUndo(), "copy into an empty register records NO undo entry (its origin)");
    }

    //== applyEdit with an EXTERNAL tree — the clipboard-paste primitive (into current AND a slot) ==
    {
        MockConsumer c; c.live = 1; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        juce::ValueTree clip ("S"); clip.setProperty ("v", 77, nullptr);   // "clipboard" content

        h.applyEdit (0, clip);                               // paste into the CURRENT register (A)
        check (c.live == 77, "applyEdit into current applies the external content to live");
        h.undo();
        check (c.live == 1, "paste-into-current is an undoable edit in the current register");

        h.applyEdit (2, clip);                               // paste into a NON-active empty register C
        h.switchTo (2);
        check (c.live == 77, "applyEdit into a slot sets that register from the external content");

        juce::ValueTree bad;                                 // invalid tree
        const int before = c.live;
        h.applyEdit (1, bad);                                // invalid content is a no-op
        check (c.live == before && ! h.registerTree (1).has_value(), "applyEdit with an invalid tree is a no-op");
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

    //== registerTree accessor + reset() (consumer-parity ops for OrbitCab wiring) ===============
    {
        MockConsumer c; c.live = 1; auto h = make (c, CH::Mode::WholeWorkspace, cfgN (4)); h.tick();
        h.switchTo (2); c.live = 88; h.switchTo (0);     // C stored = 88, active A(=1)
        check (! h.registerTree (0).has_value(), "registerTree(active) is nullopt (live is authoritative)");
        check (h.registerTree (2).has_value(), "registerTree(stored) returns the register snapshot");
        check (static_cast<int> (h.registerTree (2)->getChild (0).getProperty ("v", -1)) == 88
               || static_cast<int> (h.registerTree (2)->getProperty ("v", -1)) == 88, "registerTree carries the stored content");

        c.live = 5; settle (h, kSettle);                 // make some history + non-empty registers
        h.reset();
        // (registerEdited exercised in the PerRegister block below, where it is per-register)
        check (h.active() == 0, "reset returns to register A");
        check (! h.canUndo() && ! h.canRedo(), "reset clears all undo/redo history");
        check (! h.registerTree (1).has_value() && ! h.registerTree (2).has_value(), "reset empties every register");
        check (c.live == 5, "reset leaves the LIVE state untouched");
    }

    //== registerEdited — the "modified since dialed" marker (PerRegister) =========================
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        check (! h.registerEdited (0), "PR: a fresh register reports NOT edited");
        c.live = 7; settle (h, kSettle);
        check (h.registerEdited (0), "PR: a register with edits reports edited");
        h.switchTo (1);
        check (h.registerEdited (0) && ! h.registerEdited (1), "PR: edited-marker is PER-register (A edited, B not)");
        h.switchTo (0); h.undo();
        check (! h.registerEdited (0), "PR: undoing back out of a register clears its edited-marker");
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
                case CH::Reason::Edit:                    // never reaches onAfterApply (settle/gesture commits)
                case CH::Reason::Gesture:
                case CH::Reason::Preset:  break;
            }
        };
        h.tick();
        h.switchTo (1);                          // Switch
        c.live = 9; settle (h, kSettle); h.undo();   // Undo
        h.redo();                                // Redo
        h.copyRegister (0, 2);                   // Copy (active A into C)
        h.fromTree (h.toTree());                 // Load
        check (nSwitch == 1, "onAfterApply fires exactly once for Switch");
        check (nUndo == 1 && nRedo == 1, "onAfterApply fires once each for Undo and Redo");
        check (nCopy == 1, "onAfterApply fires once for a stamp (Copy)");
        check (nLoad == 1, "onAfterApply fires once for a load");
    }

    //== v1.1 — flush-first: undo/redo commit the pending burst before acting (Fable §0, the P1) ===
    {
        // WholeWorkspace: a settled 0->10, then an UNSETTLED 10->12; undo must revert EXACTLY 12->10.
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::WholeWorkspace, cfgN (4)); h.tick();
        c.live = 10; settle (h, kSettle);              // committed step 0->10
        c.live = 12; h.tick();                         // edit to 12, NOT settled
        h.undo();
        check (c.live == 10, "WW: undo within the settle window reverts EXACTLY the last edit (no 2-step jump)");
        check (h.canUndo(), "WW: the earlier 0->10 step survived (was never lost)");
        h.undo();
        check (c.live == 0, "WW: undoing again reaches the original state");
    }
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 10; settle (h, kSettle);
        c.live = 12; h.tick();                         // unsettled
        h.undo();
        check (c.live == 10, "PR: undo within the settle window reverts EXACTLY the last edit");
    }
    {
        // redo after an unsettled edit returns false (the fresh edit legitimately clears redo).
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 5; settle (h, kSettle); h.undo();     // back at 0, redo available
        check (h.canRedo(), "PR: redo available after undo");
        c.live = 9; h.tick();                          // fresh UNSETTLED edit on top
        check (! h.redo(), "PR: redo after an unsettled edit returns false");
        check (c.live == 9, "PR: the failed redo left the live edit in place");
    }

    //== v1.1 — WholeWorkspace switchTo is an IMMEDIATE, redo-safe step (no settle needed) =========
    {
        MockConsumer c; c.live = 10; auto h = make (c, CH::Mode::WholeWorkspace, cfgN (4)); h.tick();
        h.switchTo (1);
        h.undo();                                      // no settle in between
        check (h.active() == 0 && c.live == 10, "WW: switchTo is immediate — undo restores it with no settle");
    }

    //== v1.1 — gestures: a bracketed burst = ONE step, regardless of settle pauses ================
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.beginGesture ("drag");
        for (int v = 1; v <= 5; ++v) { c.live = v; h.tick(); settle (h, kSettle); }   // pauses INSIDE
        h.endGesture();
        check (h.undoDepth() == 1, "gesture: the whole drag (with mid-pauses) is ONE step");
        check (h.peekUndoLabel() == juce::String ("drag"), "gesture: the step carries the gesture label");
        h.undo();
        check (c.live == 0, "gesture: undo reverts the whole drag at once");
    }
    {
        // an unsettled tweak BEFORE a gesture is flushed (not absorbed) -> 2 separate steps.
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 1; h.tick();
        h.beginGesture();
        c.live = 2; h.tick();
        h.endGesture();
        check (h.undoDepth() == 2, "gesture: a pre-gesture tweak and the gesture are SEPARATE steps");
    }
    {
        // nested begin/begin/end/end = one step (the outermost owns the boundary).
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.beginGesture(); h.beginGesture();
        c.live = 7; h.tick();
        h.endGesture();
        check (h.undoDepth() == 0, "gesture: an inner endGesture does NOT commit");
        h.endGesture();
        check (h.undoDepth() == 1, "gesture: nested begin/begin/end/end is ONE step");
    }
    {
        // ScopedGesture RAII coalesces the same way.
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        { CH::ScopedGesture g (h, "wheel"); c.live = 3; h.tick(); c.live = 4; h.tick(); }
        check (h.undoDepth() == 1 && h.peekUndoLabel() == juce::String ("wheel"), "ScopedGesture: RAII bracket = one labelled step");
    }

    //== v1.1 — suppression: bracketed programmatic writes record NOTHING =========================
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        { CH::ScopedSuppress s (h); c.live = 99; h.tick(); settle (h, kSettle); }
        settle (h, kSettle);
        check (! h.canUndo(), "suppress: a suppressed burst records NO undo step");
        check (c.live == 99, "suppress: the live change stands (only recording is gated)");
    }
    {
        // suppression inside a gesture must NOT erase the gesture's own delta.
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.beginGesture();
        c.live = 3; h.tick();
        { CH::ScopedSuppress s (h); c.live = 50; h.tick(); }   // automation-like, suppressed
        c.live = 3; h.tick();                                  // net gesture delta stays 0->3
        h.endGesture();
        check (h.undoDepth() == 1, "suppress-in-gesture: the gesture still commits its own delta");
        h.undo();
        check (c.live == 0, "suppress-in-gesture: undo reverts to the gesture baseline");
    }

    //== v1.1 — flush() + clearHistory() =========================================================
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 5; h.tick();                          // unsettled
        h.flush();
        check (h.undoDepth() == 1, "flush: force-commits a pending burst immediately");
        c.live = 6; h.tick();
        h.clearHistory();
        check (! h.canUndo() && ! h.canRedo(), "clearHistory: empties undo/redo");
        check (c.live == 6, "clearHistory: leaves live untouched");
        settle (h, kSettle);
        check (h.canUndo(), "clearHistory: a pending edit survives (settles into the fresh history, not eaten)");
    }

    //== v1.1 — B4 envelope: schema + count + skip-not-clamp + bool fromTree + mismatch ===========
    {
        MockConsumer cs; cs.live = 0; auto hs = make (cs, CH::Mode::PerRegister, cfgN (6));
        hs.switchTo (3); cs.live = 33; hs.switchTo (5); cs.live = 55; hs.switchTo (0); cs.live = 100;
        const juce::ValueTree saved = hs.toTree();
        check (static_cast<int> (saved.getProperty ("count", -1)) == 6, "envelope stamps the register count");
        check (static_cast<int> (saved.getProperty ("schema", -1)) == 1, "envelope stamps the schema version");

        MockConsumer cl; auto hl = make (cl, CH::Mode::PerRegister, cfgN (4));
        int mmFrom = 0, mmTo = 0; hl.onRegisterCountMismatch = [&] (int f, int t) { mmFrom = f; mmTo = t; };
        const bool ok = hl.fromTree (saved);
        check (ok, "fromTree of a same-schema envelope returns true");
        check (mmFrom == 6 && mmTo == 4, "fromTree fires onRegisterCountMismatch(6,4)");
        check (cl.live == 100 && hl.active() == 0, "fromTree restored active + live from the 6-reg session");
        hl.switchTo (3);
        check (cl.live == 33, "register D (in range) survived the down-migration — NOT collapsed");
    }
    {
        // out-of-range active in the envelope -> a valid active, no crash, live survives.
        juce::ValueTree ws ("Workspace");
        ws.setProperty ("active", 9, nullptr);         // out of range for a 4-reg build
        juce::ValueTree live ("Live"), s ("S"); s.setProperty ("v", 7, nullptr);
        live.appendChild (s, nullptr); ws.appendChild (live, nullptr);
        MockConsumer c; auto h = make (c, CH::Mode::PerRegister, cfgN (4));
        const bool ok = h.fromTree (ws);
        check (ok, "fromTree of a legacy (no-schema) envelope loads");
        check (h.active() >= 0 && h.active() < 4, "out-of-range active resolves to a valid register");
        check (c.live == 7, "the live payload survives an out-of-range active");
    }
    {
        MockConsumer c; c.live = 42; auto h = make (c, CH::Mode::PerRegister, cfgN (4));
        check (! h.fromTree (juce::ValueTree ("NotAWorkspace")), "fromTree rejects a foreign tree (returns false)");
        check (c.live == 42, "a rejected fromTree leaves live untouched");
    }

    //== v1.1 — B5 onHistoryChanged fires on the right transitions ================================
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        int nChanged = 0; h.onHistoryChanged = [&] { ++nChanged; };
        c.live = 5; settle (h, kSettle);
        check (nChanged >= 1, "onHistoryChanged fires when a step commits");
        int before = nChanged; h.switchTo (1);
        check (nChanged > before, "onHistoryChanged fires on a PerRegister switch (active track changed)");
        before = nChanged; const bool did = h.undo();
        check (! did && nChanged == before, "onHistoryChanged does NOT fire on a genuine no-op undo");
        before = nChanged; h.applyEdit (2, c.capture());
        check (nChanged > before, "onHistoryChanged fires on a non-active applyEdit");
    }

    //== v1.1 — B6 saved/clean marker survives undo-past-save =====================================
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 1; settle (h, kSettle);
        c.live = 2; settle (h, kSettle);
        h.markSaved();
        check (! h.hasUnsavedChanges(), "saved: right after markSaved there are no unsaved changes");
        h.undo();
        check (h.hasUnsavedChanges(), "saved: an undo past the save point reads as unsaved");
        c.live = 9; settle (h, kSettle); h.markSaved();
        check (! h.hasUnsavedChanges(), "saved: markSaved re-baselines the clean point");
    }

    //== v1.1 crew-fix regressions (bugs the first 103 checks missed) =============================

    // F1 — suppression DOMINATES every commit path, not just tick().
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        {
            CH::ScopedSuppress s (h);
            CH::ScopedGesture  g (h);                  // ~ScopedGesture (endGesture) runs BEFORE ~ScopedSuppress
            c.live = 5; h.tick();
        }
        check (! h.canUndo(), "F1: a gesture ending while suppressed records no step");
        check (c.live == 5, "F1: the live change still stands");
    }
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.beginSuppress();
        c.live = 99; h.tick();
        h.beginGesture();                              // its flush must NOT commit the suppressed 0->99
        c.live = 7;  h.tick();
        h.endGesture();                                // still suppressed -> no commit
        h.endSuppress();
        check (! h.canUndo(), "F1: no commit path leaks a step while suppressed (beginGesture flush + endGesture)");
    }

    // F2 — fromTree of a <Workspace> lacking a <Live> payload is rejected without mutating.
    {
        MockConsumer c; c.live = 42; auto h = make (c, CH::Mode::PerRegister, cfgN (4));
        h.applyEdit (1, mk (7));                        // give reg B content to prove it survives
        juce::ValueTree ws ("Workspace"); ws.setProperty ("active", 0, nullptr);   // NO <Live>
        check (! h.fromTree (ws), "F2: fromTree of a Workspace lacking <Live> returns false");
        check (c.live == 42, "F2: ...and leaves live untouched");
        check (h.registerTree (1).has_value(), "F2: ...and leaves registers untouched");
    }

    // F3 — applyEdit into a NON-active register flushes the active pending burst first (§2.1).
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 1; h.tick();                          // A pending (unsettled)
        h.applyEdit (2, mk (55));                       // non-active edit — must flush A's 0->1 first
        c.live = 2; settle (h, kSettle);               // A commits 1->2
        h.undo();
        check (c.live == 1, "F3: applyEdit(non-active) flushed the active pending as a separate step");
    }

    // F4 — a no-op paste (same content) into a non-empty register adds no step / no dirty.
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.applyEdit (2, mk (5));                        // birth (no step)
        h.applyEdit (2, mk (9));                        // 5->9 : one step in C
        h.markSaved();
        h.applyEdit (2, mk (9));                        // identical -> no-op
        check (! h.hasUnsavedChanges(), "F4: a no-op paste does not mark dirty");   // check BEFORE any switch
        h.switchTo (2);
        check (h.undoDepth() == 1, "F4: a no-op paste into a non-empty register adds no undo step");
    }

    // F5 — a genuine no-op active paste fires neither onAfterApply nor onHistoryChanged.
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        int nHist = 0, nApply = 0;
        h.onHistoryChanged = [&] { ++nHist; };
        h.onAfterApply     = [&] (CH::Reason) { ++nApply; };
        h.applyEdit (0, c.capture());                  // active, byte-equal to live => no-op
        check (nHist == 0 && nApply == 0, "F5: a no-op active paste fires neither callback");
    }

    // F7 — a suppressed state change marks dirty AND clears a now-stale redo.
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 5; settle (h, kSettle); h.undo();     // back at 0, redo = [5]
        h.markSaved();
        check (h.canRedo(), "F7: redo available before the suppressed change");
        { CH::ScopedSuppress s (h); c.live = 9; h.tick(); }
        check (! h.canRedo(), "F7: a suppressed state change clears the stale redo");
        check (h.hasUnsavedChanges(), "F7: a suppressed state change marks dirty");
    }

    // §2.2 / A1 — WholeWorkspace non-active applyEdit: redo-safe + exactly one onAfterApply(Copy) (was 0 coverage).
    {
        MockConsumer c; c.live = 10; auto h = make (c, CH::Mode::WholeWorkspace, cfgN (4)); h.tick();
        int nCopy = 0; h.onAfterApply = [&] (CH::Reason r) { if (r == CH::Reason::Copy) ++nCopy; };
        c.live = 20; settle (h, kSettle); h.undo();    // live=10, redo=[20]
        h.applyEdit (1, mk (77));                       // non-active WW edit routes through commitEdit
        check (! h.canRedo(), "WW: a non-active applyEdit clears redo (redo-safe, immediate)");
        check (nCopy == 1, "WW: a non-active applyEdit fires exactly one onAfterApply(Copy)");
        h.switchTo (1);
        check (c.live == 77, "WW: the non-active applyEdit stored the content");
    }

    // WW switchTo is redoable (only undo was covered).
    {
        MockConsumer c; c.live = 10; auto h = make (c, CH::Mode::WholeWorkspace, cfgN (4)); h.tick();
        h.switchTo (1); h.undo();
        check (h.canRedo(), "WW: switchTo is redoable");
        h.redo();
        check (h.active() == 1, "WW: redo re-applies the register switch");
    }

    // B4 — upward count mismatch (fewer saved -> larger build) fires the callback too.
    {
        MockConsumer cs; cs.live = 1; auto hs = make (cs, CH::Mode::PerRegister, cfgN (4));
        const juce::ValueTree saved = hs.toTree();
        MockConsumer cl; auto hl = make (cl, CH::Mode::PerRegister, cfgN (8));
        int f = 0, t = 0; hl.onRegisterCountMismatch = [&] (int a, int b) { f = a; t = b; };
        hl.fromTree (saved);
        check (f == 4 && t == 8, "B4: onRegisterCountMismatch(4,8) fires on an upward migration");
    }

    // B4 — an unknown NEWER schema returns false (best-effort load).
    {
        MockConsumer c; c.live = 3; auto h = make (c, CH::Mode::PerRegister, cfgN (4));
        juce::ValueTree ws = h.toTree(); ws.setProperty ("schema", 2, nullptr);
        MockConsumer c2; auto h2 = make (c2, CH::Mode::PerRegister, cfgN (4));
        check (! h2.fromTree (ws), "B4: an unknown newer schema returns false (best-effort load)");
        check (c2.live == 3, "B4: ...but still applies the payload");
    }

    // B5 — an explicit applyEdit label round-trips onto the undo entry.
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.applyEdit (0, mk (8), "paste");
        check (h.peekUndoLabel() == juce::String ("paste"), "B5: an applyEdit label round-trips to the undo entry");
    }

    // clearHistory(non-active reg) never disturbs the active register's pending burst.
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.switchTo (2); c.live = 22; settle (h, kSettle); h.switchTo (0);   // C has history
        c.live = 7; h.tick();                          // A pending
        h.clearHistory (2);                            // clear ONLY C's history
        settle (h, kSettle);
        check (h.canUndo(), "clearHistory(non-active) leaves the active register's pending edit intact");
        h.undo();
        check (c.live == 0, "clearHistory(non-active): A's edit reverts cleanly");
    }

    //== round-3 crew-fix regressions: suppression records NOTHING, from EVERY path ================

    // R3.1 — explicit edits under suppression apply but record no step (the preset-load use case).
    {
        MockConsumer c; c.live = 1; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        {
            CH::ScopedSuppress s (h);
            h.applyEdit (0, mk (10));                  // active edit  -> applies, no record
            h.copyRegister (0, 2);                     // copy A into C -> no record
            h.applyEdit (3, mk (20));                  // non-active   -> no record
        }
        check (! h.canUndo(), "R3.1: applyEdit/copyRegister under suppression record no undo step");
        check (c.live == 10, "R3.1: ...the active edit still applied");
        h.switchTo (2); check (c.live == 10, "R3.1: ...the copy landed in C");
        h.switchTo (3); check (c.live == 20, "R3.1: ...the non-active edit landed in D");
    }
    // R3.2 — a real pending burst is flushed as its OWN step before suppression engages.
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 1; h.tick();                          // A pending 0->1
        { CH::ScopedSuppress s (h); c.live = 9; h.tick(); }   // flush commits 0->1, then suppresses 1->9
        check (h.canUndo(), "R3.2: the pre-suppress pending burst became its own committed step");
        check (c.live == 9, "R3.2: the suppressed change stands");
        h.undo();
        check (c.live == 0, "R3.2: undo reverts the flushed 0->1, not the suppressed 1->9");
    }
    // R3.3 — WholeWorkspace: an explicit applyEdit under suppression records nothing either.
    {
        MockConsumer c; c.live = 10; auto h = make (c, CH::Mode::WholeWorkspace, cfgN (4)); h.tick();
        { CH::ScopedSuppress s (h); h.applyEdit (0, mk (99)); }
        check (! h.canUndo(), "R3.3: WW explicit applyEdit under suppression records no step");
        check (c.live == 99, "R3.3: ...but the edit applied");
    }
    // R3.4 — CROSSED gesture/suppress: a suppressed content change still clears the stale redo + marks
    //         dirty (suppressBaseline_ is now captured unconditionally at the outermost beginSuppress).
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 5; settle (h, kSettle); h.undo();     // back at 0, redo = [5]
        h.markSaved();
        h.beginGesture();
        h.beginSuppress();                             // suppress opened INSIDE the gesture
        c.live = 8; h.tick();
        h.endGesture();                                // crossed: gesture ends before suppress
        h.endSuppress();
        check (! h.canRedo(), "R3.4: a crossed-scope suppressed change clears the stale redo");
        check (h.hasUnsavedChanges(), "R3.4: ...and marks dirty");
    }
    // R3.5 — a suppressed write to a NON-active register (PerRegister) still marks dirty AND clears
    //         that register's stale redo — endSuppress's live-diff is blind to non-active slots.
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.switchTo (2); c.live = 5; settle (h, kSettle); h.undo();   // C accrues history, then redo=[5]
        h.switchTo (0);                                              // back on A (C now stores its content)
        h.markSaved();
        const int liveBefore = c.live;
        { CH::ScopedSuppress s (h); h.applyEdit (2, mk (77)); }      // suppressed write to INACTIVE C only
        check (c.live == liveBefore, "R3.5: a suppressed non-active write leaves the active live untouched");
        check (h.hasUnsavedChanges(), "R3.5: ...yet still marks the workspace dirty");
        h.switchTo (2);
        check (c.live == 77, "R3.5: the suppressed write landed in C");
        check (! h.canRedo(), "R3.5: C's stale redo was cleared (no resurrection of pre-suppression content)");
    }
    // R3.6 — a NET-ZERO suppressed span on a non-active register reports CLEAN (endSuppress diffs the
    //         whole workspace, not a sticky flag): C returns to its pre-suppress content -> no false dirty.
    {
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.applyEdit (2, mk (5));                        // C := 5
        h.markSaved();
        {
            CH::ScopedSuppress s (h);
            h.applyEdit (2, mk (9));                    // C: 5 -> 9
            h.applyEdit (2, mk (5));                    // C: 9 -> 5  (net zero)
        }
        check (! h.hasUnsavedChanges(), "R3.6: a net-zero suppressed non-active span reports clean, not falsely dirty");
    }

   #if ! JUCE_DEBUG
    // These deliberately trip a jassert-guarded misuse path; run them only where asserts are OFF
    // (a debug build would abort on the jassertfalse). Release CI exercises the graceful recovery.
    {
        // a mutator called re-entrantly from onAfterApply is a guarded no-op, not a corruption.
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        c.live = 5; settle (h, kSettle);
        h.onAfterApply = [&] (CH::Reason) { h.undo(); };       // illegal re-entry
        const int depthBefore = h.undoDepth();
        h.undo();
        check (h.undoDepth() == depthBefore - 1, "re-entrant undo from a callback is a no-op (only the outer undo ran)");
    }
    {
        // a register switch mid-gesture force-commits the open gesture on the ORIGINAL register.
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.beginGesture();
        c.live = 5; h.tick();
        h.switchTo (1);
        check (h.active() == 1, "mid-gesture switch still switches");
        h.switchTo (0);
        check (h.canUndo(), "mid-gesture: the gesture was committed on the ORIGINAL register (A)");
        h.undo();
        check (c.live == 0, "mid-gesture: A's forced gesture-commit reverts to the gesture baseline");
    }
    {
        // flush() while a gesture is open is a no-op (the gesture owns the boundary).
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.beginGesture();
        c.live = 5; h.tick();
        h.flush();
        check (h.undoDepth() == 0, "flush mid-gesture is a no-op");
        h.endGesture();
        check (h.undoDepth() == 1, "the gesture still commits at endGesture");
    }
    {
        // history navigation inside a suppress scope is misuse (jassert in debug); release degrades gracefully.
        MockConsumer c; c.live = 0; auto h = make (c, CH::Mode::PerRegister, cfgN (4)); h.tick();
        h.beginSuppress();
        h.switchTo (1);                                // misuse under suppression
        h.endSuppress();
        check (h.active() == 1, "misuse: switchTo under suppression still switches (release-degraded)");
    }
   #endif

    std::printf (failures == 0 ? "CompareHistory: all checks passed\n" : "CompareHistory: %d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}

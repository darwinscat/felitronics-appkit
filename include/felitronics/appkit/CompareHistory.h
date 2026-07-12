// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

//==============================================================================
// felitronics::appkit::CompareHistory — the family's shared UNDO/REDO + A/B/C/D-compare engine,
// promoted from OrbitCab's proven implementation. It manages N compare registers and a settle-timer
// undo/redo timeline over an OPAQUE state snapshot: the consumer supplies captureLive()/applyLive()
// for its own live editable state (an opaque juce::ValueTree), and the engine owns the registers,
// the coalescing and the persistence envelope. App-side JUCE infra (ValueTree) — hence appkit, not
// the JUCE-free core.
//
// TWO TOPOLOGIES, one facade — chosen by the REQUIRED Mode ctor argument (there is deliberately no
// default: the two topologies diverge sharply, so a consumer must name its intent):
//  • WholeWorkspace (OrbitCab): the undo snapshot is the ENTIRE workspace — live + all registers +
//    the active index — so a register switch is itself one reversible undo step. ONE global stack.
//  • PerRegister (TabbyEQ): each register carries its OWN undo/redo history; a switch is NOT an undo
//    step (its inverse is re-selecting the slot). Undo/redo act only on the active register; recall
//    is an edit in the current register, stamp an edit in the target.
//
// CONTRACTS the opaque seam depends on (read before wiring a consumer):
//  1. captureLive() MUST be pure and byte-STABLE for an unchanged live state — the settle timer
//     compares captures with ValueTree::isEquivalentTo; a capture that walks an unordered container
//     or stamps a timestamp never settles (undo never commits, or spams). Asserted at construction.
//  2. onAfterApply MUST be READ-ONLY w.r.t. the live editable state — it fires after applyLive() to
//     let the UI re-sync (repaint, move the A/B/C/D highlight). If it WRITES params it re-enters the
//     settle machine as a fresh edit → a phantom undo step after every undo/switch. Reflect, don't push.
//  3. Message-thread only. tick()/undo()/redo()/switchTo()/recallInto()/stampInto() and the
//     persistence entry points (toTree/fromTree, reached from a host's setStateInformation) must not
//     run concurrently — if a host may call setStateInformation off the message thread, the consumer
//     marshals it onto the message thread (this engine takes no lock).
//
// Non-copyable and non-movable (the capture/apply closures pin a consumer identity): hold it as an
// in-place member or behind a unique_ptr — not in a std::vector or a move-reassigned optional.
//==============================================================================
namespace felitronics::appkit
{

class CompareHistory
{
public:
    enum class Mode   { WholeWorkspace, PerRegister };
    enum class Reason { Switch, Undo, Redo, Copy, Load };

    struct Config
    {
        int numRegisters = 4;    // A/B/C/D
        int maxUndo      = 64;    // per stack (per register in PerRegister mode)
        int settleTicks  = 12;   // ticks of no change before a burst commits (~0.4 s @ 30 Hz — a
    };                           // tick-count, NOT wall-clock: pump tick() from one steady timer

    // captureLive: snapshot the consumer's live editable state as an opaque tree (contract 1).
    // applyLive:   restore the live state from such a tree (keep param objects valid — e.g.
    //              apvts.replaceState(copy) — so editor attachments and cached pointers survive).
    // Pass Config{} at the call site for the defaults (a `= {}` default argument can't use Config's
    // nested default member initializers from within this class definition).
    CompareHistory (Mode mode,
                    std::function<juce::ValueTree()>             captureLive,
                    std::function<void (const juce::ValueTree&)> applyLive,
                    Config config)
        : mode_    (mode),
          capture_ (std::move (captureLive)),
          apply_   (std::move (applyLive)),
          cfg_     (config)
    {
        cfg_.numRegisters = std::max (1, cfg_.numRegisters);   // clamp in release too (not just jassert)
        cfg_.maxUndo      = std::max (1, cfg_.maxUndo);
        cfg_.settleTicks  = std::max (1, cfg_.settleTicks);
        registers_.assign (static_cast<size_t> (cfg_.numRegisters), std::nullopt);
        tracks_.resize (mode_ == Mode::PerRegister ? static_cast<size_t> (cfg_.numRegisters) : 1u);

        // Contract 1: capture must be byte-stable for an unchanged state, or the settle timer never
        // settles. Catch a volatile capture at bring-up rather than as "undo mysteriously does nothing".
        jassert (capture_ && apply_);
        jassert (capture_().isEquivalentTo (capture_()));
    }

    //== A/B/C/D =================================================================================
    int  numRegisters() const noexcept { return cfg_.numRegisters; }
    int  active()       const noexcept { return active_; }

    // Switch the active register: stash live into the register we leave, recall the target (a
    // never-used target keeps the current live as its seed). In WholeWorkspace mode the workspace
    // change is recorded by the settle timer (the switch is undoable). In PerRegister mode the switch
    // is NOT an undo step — but any UNSETTLED edit on the register we leave is first flushed into its
    // own history (a "tweak then click B" stays undoable), and the target track re-seeds so the
    // recalled state is its baseline, not an edit.
    void switchTo (int reg)
    {
        if (reg < 0 || reg >= cfg_.numRegisters || reg == active_)
            return;

        if (mode_ == Mode::PerRegister)
            flushActive();                                                 // don't lose a mid-edit as an undo step

        registers_[static_cast<size_t> (active_)] = capture_();            // stash the register we leave
        active_ = reg;
        if (registers_[static_cast<size_t> (reg)].has_value())
            apply_ (*registers_[static_cast<size_t> (reg)]);               // recall an existing register
        registers_[static_cast<size_t> (reg)] = std::nullopt;              // active is live now, not stored

        if (mode_ == Mode::PerRegister)
            reseed();                                                      // the switch is not an edit in reg's history
        if (onAfterApply)
            onAfterApply (Reason::Switch);
    }

    // Recall another register's contents INTO the current one — an EDIT to the current register
    // (it overwrites live, not otherwise reversible). Committed IMMEDIATELY as one discrete undo
    // step (does not depend on the settle timer). Source register untouched.
    void recallInto (int fromReg)
    {
        if (fromReg < 0 || fromReg >= cfg_.numRegisters || fromReg == active_)
            return;
        if (const auto& src = registers_[static_cast<size_t> (fromReg)]; src.has_value())
            commitEdit (Reason::Copy, [this, &fromReg] { apply_ (*registers_[static_cast<size_t> (fromReg)]); });
    }

    // Stamp the current live state OUT into another register — an edit to THAT register's stored
    // content, undoable within it (switch there and undo restores the old contents). Live/active
    // untouched. Stamping into a NEVER-USED register is its "birth": no undo entry (nothing to
    // restore to), mirroring the fresh-register seed rule.
    void stampInto (int toReg)
    {
        if (toReg < 0 || toReg >= cfg_.numRegisters || toReg == active_)
            return;
        if (mode_ == Mode::PerRegister)
        {
            if (auto& old = registers_[static_cast<size_t> (toReg)]; old.has_value())
            {
                Track& tr = tracks_[static_cast<size_t> (toReg)];
                tr.undo.push_back (*old);                                  // only a REAL prior content is undoable
                trimStack (tr.undo);
                tr.redo.clear();
            }
        }
        registers_[static_cast<size_t> (toReg)] = capture_();
        if (onAfterApply)
            onAfterApply (Reason::Copy);
    }

    //== undo / redo (settle-timer coalescing) ===================================================
    bool canUndo() const noexcept { return ! activeTrack().undo.empty(); }
    bool canRedo() const noexcept { return ! activeTrack().redo.empty(); }

    // Pump from ONE steady UI timer. A burst of edits commits as a single step once the captured
    // state has been unchanged for settleTicks ticks (so a slider drag = one undo step).
    void tick()
    {
        Track& tr = activeTrack();
        const juce::ValueTree cur = captureSnapshot();

        if (! tr.baseline.isValid())                        // first tick: seed the baseline
        {
            tr.baseline = tr.prev = cur;
            tr.settle = 0;
            return;
        }
        if (! cur.isEquivalentTo (tr.prev))                 // still changing → wait for it to settle
        {
            tr.prev = cur;
            tr.settle = 0;
            return;
        }
        if (! cur.isEquivalentTo (tr.baseline) && ++tr.settle >= cfg_.settleTicks)
        {
            tr.undo.push_back (tr.baseline);                // commit the burst as one step
            trimStack (tr.undo);
            tr.redo.clear();
            tr.baseline = cur;
            tr.settle = 0;
        }
    }

    bool undo()
    {
        Track& tr = activeTrack();
        if (tr.undo.empty())
            return false;
        tr.redo.push_back (captureSnapshot());
        trimStack (tr.redo);
        const juce::ValueTree target = tr.undo.back();
        tr.undo.pop_back();
        applySnapshot (target);
        reseed();                                           // apply∘capture may not be identity → avoid a phantom step
        if (onAfterApply)
            onAfterApply (Reason::Undo);
        return true;
    }

    bool redo()
    {
        Track& tr = activeTrack();
        if (tr.redo.empty())
            return false;
        tr.undo.push_back (captureSnapshot());
        trimStack (tr.undo);
        const juce::ValueTree target = tr.redo.back();
        tr.redo.pop_back();
        applySnapshot (target);
        reseed();
        if (onAfterApply)
            onAfterApply (Reason::Redo);
        return true;
    }

    //== persistence (the whole workspace: live + registers + active) ============================
    // Envelope shape (byte-identical to OrbitCab's Workspace tree so a promoted consumer round-trips
    // unchanged): <Workspace active=n><Live>{live}</Live><Snaps><Snap i=0>[{reg}]</Snap>…</Snaps>
    // </Workspace>. The {live}/{reg} payloads are the consumer's opaque capture trees. Undo/redo
    // stacks are NOT persisted (a host reload is a natural history boundary).
    juce::ValueTree toTree() const
    {
        juce::ValueTree t (kWorkspace);
        t.setProperty (kActive, juce::jlimit (0, cfg_.numRegisters - 1, active_), nullptr);

        juce::ValueTree live (kLive);
        live.appendChild (capture_(), nullptr);
        t.appendChild (live, nullptr);

        juce::ValueTree snaps (kSnaps);
        for (int i = 0; i < cfg_.numRegisters; ++i)
        {
            juce::ValueTree snap (kSnap);
            snap.setProperty (kIndex, i, nullptr);
            // the active register IS live — never stored twice (defensive: skip it even if the
            // invariant were somehow violated, so the byte-identical envelope can't drift).
            if (i != active_)
                if (const auto& r = registers_[static_cast<size_t> (i)]; r.has_value())
                    snap.appendChild (r->createCopy(), nullptr);
            snaps.appendChild (snap, nullptr);
        }
        t.appendChild (snaps, nullptr);
        return t;
    }

    // Restore a workspace envelope: apply the live payload, refill the registers, re-null the active
    // register (the "active is live, never stored twice" invariant), then clear all history.
    void fromTree (const juce::ValueTree& t)
    {
        if (! t.hasType (kWorkspace))
            return;
        restoreWorkspaceBody (t);
        for (auto& tr : tracks_)
            tr = Track {};                                                 // a load is a fresh history boundary
        if (onAfterApply)
            onAfterApply (Reason::Load);
    }

    // Fired after any apply (switch / undo / redo / recall / stamp / load) so the editor re-syncs
    // its UI. MUST be read-only w.r.t. the live editable state (contract 2).
    std::function<void (Reason)> onAfterApply;

private:
    struct Track
    {
        std::vector<juce::ValueTree> undo, redo;
        juce::ValueTree              baseline, prev;
        int                          settle = 0;
    };

    Track&       activeTrack()       noexcept { return tracks_[trackIndex()]; }
    const Track& activeTrack() const noexcept { return tracks_[trackIndex()]; }
    size_t       trackIndex()  const noexcept
    {
        return mode_ == Mode::PerRegister ? static_cast<size_t> (active_) : 0u;
    }

    juce::ValueTree captureSnapshot() const
    {
        return mode_ == Mode::WholeWorkspace ? toTree() : capture_();
    }

    void applySnapshot (const juce::ValueTree& snap)
    {
        if (mode_ == Mode::WholeWorkspace)
            restoreWorkspaceBody (snap);                    // undo/redo replay: no history-clear, no callback
        else
            apply_ (snap);
    }

    // The one restore body shared by fromTree() (public load) and applySnapshot() (the WholeWorkspace
    // undo/redo REPLAY path — the exact OrbitCab NULL path). Keeping it single means the replay can
    // never silently drift from a fresh load.
    void restoreWorkspaceBody (const juce::ValueTree& t)
    {
        if (! t.hasType (kWorkspace))
            return;
        active_ = juce::jlimit (0, cfg_.numRegisters - 1, static_cast<int> (t.getProperty (kActive, 0)));
        for (auto& r : registers_)
            r = std::nullopt;
        if (const auto snaps = t.getChildWithName (kSnaps); snaps.isValid())
            for (const auto snap : snaps)
            {
                const int i = juce::jlimit (0, cfg_.numRegisters - 1, static_cast<int> (snap.getProperty (kIndex, 0)));
                if (i != active_ && snap.getNumChildren() > 0)
                    registers_[static_cast<size_t> (i)] = snap.getChild (0).createCopy();
            }
        registers_[static_cast<size_t> (active_)] = std::nullopt;          // active is live, never stored
        if (const auto live = t.getChildWithName (kLive); live.isValid() && live.getNumChildren() > 0)
            apply_ (live.getChild (0).createCopy());
    }

    // Force-commit any pending burst on the active track as ONE undo step. Used at edit boundaries
    // (switching away, a recall) so a not-yet-settled edit is not lost from history.
    void flushActive()
    {
        Track& tr = activeTrack();
        if (! tr.baseline.isValid())
            return;
        const juce::ValueTree cur = captureSnapshot();
        if (! cur.isEquivalentTo (tr.baseline))
        {
            tr.undo.push_back (tr.baseline);
            trimStack (tr.undo);
            tr.redo.clear();
            tr.baseline = tr.prev = cur;
            tr.settle = 0;
        }
    }

    // Apply a discrete edit and commit it as ONE undo step immediately (independent of the settle
    // timer): flush any prior pending burst, snapshot before/after, record the before if it changed.
    template <class ApplyFn>
    void commitEdit (Reason reason, ApplyFn&& applyEdit)
    {
        flushActive();
        Track& tr = activeTrack();
        const juce::ValueTree before = captureSnapshot();
        applyEdit();
        const juce::ValueTree after = captureSnapshot();
        if (! after.isEquivalentTo (before))
        {
            tr.undo.push_back (before);
            trimStack (tr.undo);
            tr.redo.clear();
            tr.baseline = tr.prev = after;
            tr.settle = 0;
        }
        if (onAfterApply)
            onAfterApply (reason);
    }

    void reseed()
    {
        Track& tr = activeTrack();
        tr.baseline = tr.prev = captureSnapshot();
        tr.settle = 0;
    }

    void trimStack (std::vector<juce::ValueTree>& s) const
    {
        while (static_cast<int> (s.size()) > cfg_.maxUndo)
            s.erase (s.begin());
    }

    static constexpr const char* kWorkspace = "Workspace";
    static constexpr const char* kLive      = "Live";
    static constexpr const char* kSnaps     = "Snaps";
    static constexpr const char* kSnap      = "Snap";
    static constexpr const char* kActive    = "active";
    static constexpr const char* kIndex     = "i";

    Mode                                         mode_;
    std::function<juce::ValueTree()>             capture_;
    std::function<void (const juce::ValueTree&)> apply_;
    Config                                       cfg_;
    std::vector<std::optional<juce::ValueTree>>  registers_;   // active slot = nullopt (live is authoritative)
    int                                          active_ = 0;
    std::vector<Track>                           tracks_;      // 1 (WholeWorkspace) or numRegisters (PerRegister)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompareHistory)
};

} // namespace felitronics::appkit

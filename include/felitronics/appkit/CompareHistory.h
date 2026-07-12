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
// promoted from OrbitCab's proven implementation and matured (v1.1) across a 5-reviewer crew pass.
// It manages N compare registers and a settle-timer undo/redo timeline over an OPAQUE state
// snapshot: the consumer supplies captureLive()/applyLive() for its own live editable state (an
// opaque juce::ValueTree), and the engine owns the registers, the coalescing, the gesture/
// suppression scopes, and the persistence envelope. App-side JUCE infra (ValueTree) — hence appkit.
//
// TWO TOPOLOGIES, one facade — chosen by the REQUIRED Mode ctor argument (no default: the two
// topologies diverge sharply, so a consumer must name its intent):
//  • WholeWorkspace (OrbitCab legacy): the undo snapshot is the ENTIRE workspace — live + all
//    registers + the active index — so a register switch is itself one reversible undo step. ONE
//    global stack.
//  • PerRegister (TabbyEQ): each register carries its OWN undo/redo history; a switch is NOT an undo
//    step (its inverse is re-selecting the slot). Undo/redo act only on the active register.
//
// EDIT COALESCING — three ways, in precedence order:
//  1. ScopedGesture (begin/endGesture): the RIGHT tool for a UI drag. Everything between begin and
//     end is ONE undo step, regardless of settle-timer pauses. Nestable (refcounted).
//  2. ScopedSuppress (begin/endSuppress): record NOTHING — for programmatic bulk writes (preset
//     apply, band reset, a fromTree call site). NOTE: continuous DAW automation is NOT reliably
//     bracketable (it interleaves with UI writes on one live tree); suppression is a programmatic-
//     write gate, it does not promise "Ctrl+Z never unwinds automation". Suppression DOMINATES a
//     gesture (while suppressed, nothing commits).
//  3. The settle timer (tick): the fallback for un-bracketed edits — a burst that has been stable
//     for settleTicks ticks commits as one step.
//
// CONTRACTS the opaque seam depends on (read before wiring a consumer):
//  1. captureLive() MUST be pure, byte-STABLE for an unchanged live state (the settle timer compares
//     captures with isEquivalentTo — a capture that walks an unordered container or stamps a
//     timestamp never settles), AND must return an INDEPENDENT deep snapshot, not a handle aliasing
//     the live tree (else a later live mutation also mutates a stored baseline and undo records
//     nothing). Stability is asserted at construction.
//  2. onAfterApply MUST be READ-ONLY w.r.t. the live editable state — it fires after applyLive() to
//     let the UI re-sync. If it WRITES params it re-enters the settle machine as a fresh edit → a
//     phantom undo step. Enforced in debug (capture stability across the callback) and guarded
//     against re-entrancy (inApply_); a callback that writes ASYNChronously (triggerAsyncUpdate) is
//     NOT caught — put normalization/clamping-on-load in applyLive(), never here.
//  3. Message-thread only. Every mutating entry point and the persistence entry points must not run
//     concurrently. This engine takes NO lock and (deliberately, to stay juce_data_structures-only)
//     does NOT assert the thread — if a host may call setStateInformation off the message thread,
//     the consumer marshals it onto the message thread.
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
    // Why an edit exists — carried on each history Entry and passed to onAfterApply for the apply
    // reasons. Edit = an ordinary settle/gesture-less commit (never reaches onAfterApply). Adding a
    // value later would break consumers' -Wswitch-enum switches, so the full set lives here now.
    enum class Reason { Edit, Switch, Undo, Redo, Copy, Load, Gesture, Preset };

    struct Config
    {
        int numRegisters = 4;    // A/B/C/D
        int maxUndo      = 64;    // per stack (per register in PerRegister mode)
        int settleTicks  = 12;   // ticks of no change before a burst commits (~0.4 s @ 30 Hz — a
    };                           // tick-count, NOT wall-clock: pump tick() from one steady timer

    // captureLive: snapshot the consumer's live editable state as an opaque, independent tree (c.1).
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

        reseed();   // seed the active baseline NOW so an edit before the first tick() is still undoable
    }

    //== A/B/C/D =================================================================================
    int  numRegisters() const noexcept { return cfg_.numRegisters; }
    int  active()       const noexcept { return active_; }

    // A register's stored snapshot, or nullopt for an empty / the active register (whose content is
    // live, not stored). For a consumer that must inspect register contents — e.g. to embed the
    // external assets a register references into the session save, or draw an A/B/C/D preview.
    const std::optional<juce::ValueTree>& registerTree (int reg) const noexcept
    {
        jassert (reg >= 0 && reg < cfg_.numRegisters);
        return registers_[static_cast<size_t> (reg)];
    }

    // Has this register accumulated edits (its own undo history is non-empty)? The "modified since
    // you dialed this in" marker for the A/B/C/D UI. In WholeWorkspace mode there is one shared
    // history, so this reports the same for every register.
    bool registerEdited (int reg) const noexcept
    {
        jassert (reg >= 0 && reg < cfg_.numRegisters);
        const size_t t = mode_ == Mode::PerRegister ? static_cast<size_t> (reg) : 0u;
        return ! tracks_[t].undo.empty();
    }

    // Reset to a clean workspace — active register 0, every register empty, all history cleared,
    // all scopes closed. The LIVE state is left UNTOUCHED (the consumer sets it around this call,
    // e.g. a factory default). No apply, no onAfterApply; fires onHistoryChanged. Marks dirty
    // (a factory reset changes the workspace) — the consumer may markSaved() after if appropriate.
    void reset()
    {
        if (inApply_) { jassertfalse; return; }
        const ScopedFlag guard (inApply_);

        jassert (gestureDepth_ == 0 && suppressDepth_ == 0); // don't reset inside an open gesture/suppress scope
        gestureDepth_ = 0; suppressDepth_ = 0; gestureLabel_.clear();
        active_ = 0;
        for (auto& r : registers_)
            r = std::nullopt;
        for (auto& tr : tracks_)
            tr = Track {};
        reseed();
        ++editSerial_;
        fireHistoryChanged();
    }

    // Switch the active register (stash live into the register we leave, recall the target; a
    // never-used target keeps the current live as its seed). WholeWorkspace: the switch is ONE
    // immediate, redo-safe undo step. PerRegister: the switch is NOT an undo step, but any UNSETTLED
    // edit on the register we leave is first flushed into its own history.
    void switchTo (int reg)
    {
        if (inApply_) { jassertfalse; return; }
        const ScopedFlag guard (inApply_);
        if (reg < 0 || reg >= cfg_.numRegisters || reg == active_)
            return;
        if (suppressDepth_ > 0) jassertfalse;                // a register switch inside a suppress scope is misuse
        closeOpenGestureOnMisuse();                          // a switch mid-drag would corrupt the track

        if (mode_ == Mode::PerRegister)
        {
            commitPendingBurst();                            // don't lose a mid-edit as an undo step
            doSwapTo (reg);
            reseed();                                        // the switch is not an edit in reg's history
            ++editSerial_;
            fireAfterApply (Reason::Switch);
            fireHistoryChanged();
        }
        else
        {
            commitEdit (Reason::Switch, {}, [this, reg] { doSwapTo (reg); });   // immediate + redo-safe
        }
    }

    // Apply arbitrary CONTENT into a register as ONE discrete undoable edit — the single primitive
    // behind every copy/paste (register→register, or an external clipboard tree). If toReg is the
    // ACTIVE register the content becomes live; otherwise it overwrites the stored register.
    // Overwriting a NEVER-USED register in PerRegister mode is its "birth": no undo entry (in
    // WholeWorkspace filling a register is a workspace change, so it IS one step — an intentional
    // asymmetry). `content` is copied; an invalid tree is a no-op. Fires onAfterApply(Copy).
    void applyEdit (int toReg, const juce::ValueTree& content, const juce::String& label = {})
    {
        if (inApply_) { jassertfalse; return; }
        const ScopedFlag guard (inApply_);
        closeOpenGestureOnMisuse();
        applyEditImpl (toReg, content, Reason::Copy, label);
    }

    // Copy one register's content INTO another as a discrete undoable edit in the target — the
    // internal-source case of applyEdit ("copy here from X" = copyRegister(X, current); "copy this
    // to Y" = copyRegister(current, Y)). No-op if the source is empty or the same slot.
    void copyRegister (int from, int to)
    {
        if (inApply_) { jassertfalse; return; }
        const ScopedFlag guard (inApply_);
        if (from < 0 || from >= cfg_.numRegisters || from == to)
            return;
        closeOpenGestureOnMisuse();
        const juce::ValueTree src = (from == active_)
                                        ? capture_()
                                        : registers_[static_cast<size_t> (from)].value_or (juce::ValueTree());
        applyEditImpl (to, src, Reason::Copy, {});
    }

    //== gesture / suppression scopes ============================================================
    // Open/close a gesture: everything between is ONE labelled undo step (the tool for a UI drag).
    // Nestable — the OUTERMOST begin captures the baseline and its label; the outermost end commits.
    void beginGesture (const juce::String& label = {})
    {
        if (inApply_) { jassertfalse; return; }
        const ScopedFlag guard (inApply_);
        if (gestureDepth_++ == 0)
        {
            commitPendingBurst();                            // don't absorb a prior burst into the gesture
            reseed();                                        // baseline = state at gesture start
            gestureLabel_ = label;
        }
    }

    void endGesture()
    {
        if (inApply_) { jassertfalse; return; }
        const ScopedFlag guard (inApply_);
        if (gestureDepth_ == 0) { jassertfalse; return; }    // unbalanced end
        if (--gestureDepth_ > 0)
            return;                                          // inner end: keep coalescing
        commitGesture();
    }

    // Suppress recording: while any suppression is open the engine records NO undo history for ANY
    // change — settle drift, gestures, AND explicit applyEdit/copyRegister (they still apply their
    // content, they just don't create a step). Dominates a gesture. The suppressed span is one
    // non-undoable forward move: the outermost endSuppress clears a now-stale redo + marks dirty if
    // the content moved. CONTRACT: wrap the consumer's own live-state writes only — do NOT call the
    // history NAVIGATION/LOAD ops (undo/redo/switchTo/fromTree/reset) inside a suppress scope (they
    // jassert; a multi-register load is fromTree, not a suppressed sequence of switchTo).
    void beginSuppress()
    {
        if (inApply_) { jassertfalse; return; }
        const ScopedFlag guard (inApply_);
        if (suppressDepth_ == 0)
        {
            if (gestureDepth_ == 0)                          // flush BEFORE entering suppression (commitPendingBurst
                commitPendingBurst();                        // is still ungated here) — but never a gesture's pending
            suppressBaseline_   = captureSnapshot();         // active-live baseline (clears the active redo on drift)
            suppressWsBaseline_ = toTree();                  // whole-workspace baseline (dirty; sees non-active slots).
        }                                                    // Captured ALWAYS — a gesture-crossed scope must not read stale
        ++suppressDepth_;
    }

    void endSuppress()
    {
        if (inApply_) { jassertfalse; return; }
        const ScopedFlag guard (inApply_);
        if (suppressDepth_ == 0) { jassertfalse; return; }
        if (--suppressDepth_ == 0)
        {
            // A suppressed write is not undoable — but it DID move the timeline forward, so a stale
            // redo no longer applies and the workspace is dirty. Two orthogonal diffs, both net-zero-
            // correct: the ACTIVE-live drift clears the active track's redo; the WHOLE-workspace drift
            // (which also sees non-active register slots) marks dirty + notifies, exactly once.
            const bool activeMoved = suppressBaseline_.isValid()
                                     && ! captureSnapshot().isEquivalentTo (suppressBaseline_);
            const bool wsMoved     = suppressWsBaseline_.isValid()
                                     && ! toTree().isEquivalentTo (suppressWsBaseline_);
            if (activeMoved)
                activeTrack().redo.clear();
            if (wsMoved)
            {
                ++editSerial_;
                fireHistoryChanged();
            }
            if (gestureDepth_ == 0)
                reseed();                                    // absorb the drift (unless a gesture still owns the baseline)
        }
    }

    // RAII wrappers (prefer these to the raw begin/end).
    struct ScopedGesture
    {
        CompareHistory& h;
        explicit ScopedGesture (CompareHistory& host, const juce::String& label = {}) : h (host) { h.beginGesture (label); }
        ~ScopedGesture() { h.endGesture(); }
        JUCE_DECLARE_NON_COPYABLE (ScopedGesture)
    };
    struct ScopedSuppress
    {
        CompareHistory& h;
        explicit ScopedSuppress (CompareHistory& host) : h (host) { h.beginSuppress(); }
        ~ScopedSuppress() { h.endSuppress(); }
        JUCE_DECLARE_NON_COPYABLE (ScopedSuppress)
    };

    //== undo / redo (settle-timer coalescing + explicit scopes) =================================
    bool canUndo() const noexcept { return ! activeTrack().undo.empty(); }
    bool canRedo() const noexcept { return ! activeTrack().redo.empty(); }

    int  undoDepth() const noexcept { return static_cast<int> (activeTrack().undo.size()); }
    int  redoDepth() const noexcept { return static_cast<int> (activeTrack().redo.size()); }
    // Label of the step undo()/redo() would act on ("" for an unlabelled settle-burst / gesture).
    juce::String peekUndoLabel() const { const auto& u = activeTrack().undo; return u.empty() ? juce::String() : u.back().label; }
    juce::String peekRedoLabel() const { const auto& r = activeTrack().redo; return r.empty() ? juce::String() : r.back().label; }

    // Pump from ONE steady UI timer. A burst of edits commits as a single step once the captured
    // state has been unchanged for settleTicks ticks. No-op while a gesture is open (the gesture owns
    // the boundary) or while suppressed (nothing records; the baseline follows silently).
    void tick()
    {
        if (inApply_) return;                                // a tick from within a callback is a no-op
        const ScopedFlag guard (inApply_);

        Track& tr = activeTrack();
        const juce::ValueTree cur = captureSnapshot();

        if (! tr.baseline.isValid())                         // safety: seed the baseline
        {
            tr.baseline = tr.prev = cur;
            tr.settle = 0;
            return;
        }
        if (suppressDepth_ > 0)
        {
            if (gestureDepth_ == 0) { tr.baseline = tr.prev = cur; tr.settle = 0; }  // slide silently
            return;                                          // suppression: never commit
        }
        if (gestureDepth_ > 0)
        {
            tr.prev = cur;                                   // gesture holds its baseline; no auto-commit
            return;
        }
        if (! cur.isEquivalentTo (tr.prev))                  // still changing → wait for it to settle
        {
            tr.prev = cur;
            tr.settle = 0;
            return;
        }
        if (! cur.isEquivalentTo (tr.baseline) && ++tr.settle >= cfg_.settleTicks)
        {
            pushUndo (tr, { tr.baseline, {}, Reason::Edit });
            tr.baseline = cur;
            tr.settle = 0;
            ++editSerial_;
            fireHistoryChanged();
        }
    }

    bool undo()
    {
        if (inApply_) { jassertfalse; return false; }
        const ScopedFlag guard (inApply_);
        if (suppressDepth_ > 0) jassertfalse;                // history NAVIGATION inside a suppress scope is misuse
        closeOpenGestureOnMisuse();
        commitPendingBurst();                                // so undo reverts EXACTLY the last edit
        Track& tr = activeTrack();
        if (tr.undo.empty())
            return false;
        const Entry target = tr.undo.back();
        tr.undo.pop_back();
        tr.redo.push_back ({ captureSnapshot(), target.label, target.reason });
        trimStack (tr.redo);
        applySnapshot (target.tree);
        reseed();                                            // apply∘capture may not be identity → no phantom
        ++editSerial_;
        fireAfterApply (Reason::Undo);
        fireHistoryChanged();
        return true;
    }

    bool redo()
    {
        if (inApply_) { jassertfalse; return false; }
        const ScopedFlag guard (inApply_);
        if (suppressDepth_ > 0) jassertfalse;                // history NAVIGATION inside a suppress scope is misuse
        closeOpenGestureOnMisuse();
        commitPendingBurst();                                // a pending edit legitimately clears redo
        Track& tr = activeTrack();
        if (tr.redo.empty())
            return false;
        const Entry target = tr.redo.back();
        tr.redo.pop_back();
        tr.undo.push_back ({ captureSnapshot(), target.label, target.reason });
        trimStack (tr.undo);
        applySnapshot (target.tree);
        reseed();
        ++editSerial_;
        fireAfterApply (Reason::Redo);
        fireHistoryChanged();
        return true;
    }

    // Force-commit any pending burst on the active track NOW (an edit boundary the consumer knows
    // about). No-op while a gesture is open (you cannot force-commit half a gesture).
    void flush()
    {
        if (inApply_) { jassertfalse; return; }
        const ScopedFlag guard (inApply_);
        if (gestureDepth_ > 0) { jassertfalse; return; }
        commitPendingBurst();
    }

    // Drop undo/redo history WITHOUT touching registers, live, or the pending burst (unlike reset()).
    // reg < 0 (or WholeWorkspace) clears every track; otherwise just that register's track. Only the
    // committed stacks are wiped — a burst in flight stays pending and will settle into the now-empty
    // history (so clearing does NOT eat a mid-edit, and clearing a NON-active register never disturbs
    // the active track). Does NOT mark dirty (no content change). Use for a load/preset that should be
    // a fresh history boundary but keep the dial-in.
    void clearHistory (int reg = -1)
    {
        if (inApply_) { jassertfalse; return; }
        const ScopedFlag guard (inApply_);
        if (mode_ == Mode::WholeWorkspace || reg < 0)
            for (auto& tr : tracks_) { tr.undo.clear(); tr.redo.clear(); }
        else if (reg < static_cast<int> (tracks_.size()))
        { tracks_[static_cast<size_t> (reg)].undo.clear(); tracks_[static_cast<size_t> (reg)].redo.clear(); }
        fireHistoryChanged();
    }

    //== saved / clean marker ====================================================================
    // A monotonic edit serial, distinct from registerEdited: markSaved() records the current serial;
    // hasUnsavedChanges() is true whenever the state has moved since (including via undo/redo — the
    // conservative, never-falsely-clean choice: undoing exactly back to the saved CONTENT still
    // reads dirty). Every committed step, undo, redo, switch, copy and reset advances the serial;
    // fromTree() re-baselines it (a freshly loaded session is clean).
    void markSaved()               noexcept { if (inApply_) return; savedSerial_ = editSerial_; }
    bool hasUnsavedChanges() const noexcept { return editSerial_ != savedSerial_; }

    //== persistence (the whole workspace: live + registers + active) ============================
    // Envelope: <Workspace schema=1 count=n active=k><Live>{live}</Live><Snaps><Snap i=0>[{reg}]
    // </Snap>…</Snaps></Workspace>. {live}/{reg} are the consumer's opaque capture trees. Undo/redo
    // stacks are NOT persisted (a host reload is a natural history boundary).
    juce::ValueTree toTree() const
    {
        juce::ValueTree t (kWorkspace);
        t.setProperty (kSchema, kSchemaVersion, nullptr);
        t.setProperty (kCount,  cfg_.numRegisters, nullptr);
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
            // invariant were somehow violated, so the envelope can't drift).
            if (i != active_)
                if (const auto& r = registers_[static_cast<size_t> (i)]; r.has_value())
                    snap.appendChild (r->createCopy(), nullptr);
            snaps.appendChild (snap, nullptr);
        }
        t.appendChild (snaps, nullptr);
        return t;
    }

    // Restore a workspace envelope. Returns false for a non-Workspace tree (no state change) or an
    // unknown NEWER schema (best-effort load still applied, but the caller is told). A saved register
    // count that differs from this build's fires onRegisterCountMismatch (both directions); registers
    // beyond this build are DROPPED, not collapsed. Clears all history + closes all scopes; the load
    // is a fresh, clean boundary.
    bool fromTree (const juce::ValueTree& t)
    {
        if (inApply_) { jassertfalse; return false; }
        const ScopedFlag guard (inApply_);
        // Reject a foreign OR malformed envelope BEFORE mutating anything — a <Workspace> with no
        // <Live> payload is not loadable; clearing registers/history for it would corrupt state and
        // wrongly report success. Validate first, mutate second.
        if (! t.hasType (kWorkspace))
            return false;
        const auto liveChild = t.getChildWithName (kLive);
        if (! liveChild.isValid() || liveChild.getNumChildren() == 0)
            return false;

        jassert (gestureDepth_ == 0 && suppressDepth_ == 0); // don't load inside an open gesture/suppress scope
        gestureDepth_ = 0; suppressDepth_ = 0; gestureLabel_.clear();
        const int savedCount = t.hasProperty (kCount) ? static_cast<int> (t.getProperty (kCount))
                                                      : inferCount (t);        // legacy v0: infer
        restoreWorkspaceBody (t);
        for (auto& tr : tracks_)
            tr = Track {};                                                     // a load is a fresh boundary
        reseed();
        fireAfterApply (Reason::Load);
        fireHistoryChanged();

        if (savedCount != cfg_.numRegisters && onRegisterCountMismatch)
            onRegisterCountMismatch (savedCount, cfg_.numRegisters);

        savedSerial_ = editSerial_;                                            // a loaded session is clean
        return static_cast<int> (t.getProperty (kSchema, 0)) <= kSchemaVersion;
    }

    //== callbacks ===============================================================================
    // After any apply (switch / undo / redo / copy / load) — re-sync the UI. MUST be read-only
    // w.r.t. the live editable state (contract 2). NEVER fired for a plain Edit/Gesture commit.
    std::function<void (Reason)> onAfterApply;
    // Whenever the reported active-track depth OR the active index may have changed — replace UI
    // polling of canUndo/canRedo/undoDepth with this.
    std::function<void()> onHistoryChanged;
    // A loaded session's register count differs from this build's: (savedCount, thisBuildCount).
    // Fired from fromTree() after the state is applied. Like onAfterApply it runs UNDER the re-entrancy
    // guard, so it must NOT call engine mutators synchronously (they no-op) — warn/log, or defer any
    // migration to after fromTree() returns.
    std::function<void (int savedCount, int buildCount)> onRegisterCountMismatch;

private:
    struct Entry
    {
        juce::ValueTree tree;
        juce::String    label;
        Reason          reason = Reason::Edit;
    };
    struct Track
    {
        std::vector<Entry> undo, redo;
        juce::ValueTree    baseline, prev;
        int                settle = 0;
    };

    // Sets a bool for its lifetime — the re-entrancy guard on every public mutator body, so a
    // mutating call from within onAfterApply/onHistoryChanged (or a pathological applyLive) is a
    // no-op instead of corrupting the stack.
    struct ScopedFlag
    {
        bool& f;
        explicit ScopedFlag (bool& b) : f (b) { f = true; }
        ~ScopedFlag() { f = false; }
        JUCE_DECLARE_NON_COPYABLE (ScopedFlag)
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
            restoreWorkspaceBody (snap);                     // undo/redo replay: no history-clear, no callback
        else
            apply_ (snap);
    }

    // The raw register swap (no history): stash live into the leaving register, recall the target,
    // re-null the target (active is live now). Used by switchTo (wrapped in a commit for WW).
    void doSwapTo (int reg)
    {
        registers_[static_cast<size_t> (active_)] = capture_();
        active_ = reg;
        if (registers_[static_cast<size_t> (reg)].has_value())
            apply_ (*registers_[static_cast<size_t> (reg)]);
        registers_[static_cast<size_t> (reg)] = std::nullopt;
    }

    // The copy/paste body, private so public applyEdit/copyRegister can share it under ONE re-entrancy
    // guard (a public method calling another public method would trip the guard).
    void applyEditImpl (int toReg, const juce::ValueTree& content, Reason reason, const juce::String& label)
    {
        if (toReg < 0 || toReg >= cfg_.numRegisters || ! content.isValid())
            return;
        if (toReg == active_)
        {
            commitEdit (reason, label, [this, &content] { apply_ (content); });   // edit the active register
            return;
        }
        auto& slot = registers_[static_cast<size_t> (toReg)];
        if (mode_ == Mode::PerRegister)
        {
            commitPendingBurst();                            // §2.1 flush-first — the call IS a history op (gated under suppress)
            if (slot.has_value() && slot->isEquivalentTo (content))
                return;                                      // no-op paste (same content) — no bogus step/dirty
            if (suppressDepth_ > 0)                          // suppressed: apply the content, record NOTHING — but a
            {                                                // real change still invalidates THIS register's redo (the
                tracks_[static_cast<size_t> (toReg)].redo.clear();   // dirty flag is handled by endSuppress's whole-
                slot = content.createCopy();                 // workspace diff, which sees the slot AND nets to zero)
                return;
            }
            if (slot.has_value())                            // a REAL prior content is undoable in toReg
                pushUndo (tracks_[static_cast<size_t> (toReg)], { *slot, label, reason });
            slot = content.createCopy();                     // (empty slot = birth: no undo entry)
            ++editSerial_;
            fireAfterApply (reason);
            fireHistoryChanged();
        }
        else                                                 // WholeWorkspace: a non-active edit is one
        {                                                    // immediate, redo-safe workspace step
            commitEdit (reason, label, [&slot, &content] { slot = content.createCopy(); });
        }
    }

    void restoreWorkspaceBody (const juce::ValueTree& t)
    {
        if (! t.hasType (kWorkspace))
            return;
        int a = static_cast<int> (t.getProperty (kActive, 0));
        if (a < 0 || a >= cfg_.numRegisters)                 // out-of-range active → 0 (skip, don't clamp-collapse)
            a = 0;
        active_ = a;
        for (auto& r : registers_)
            r = std::nullopt;
        if (const auto snaps = t.getChildWithName (kSnaps); snaps.isValid())
            for (const auto snap : snaps)
            {
                const int i = static_cast<int> (snap.getProperty (kIndex, -1));
                if (i < 0 || i >= cfg_.numRegisters)         // DROP an out-of-range register (was jlimit-collapse)
                    continue;
                if (i != active_ && snap.getNumChildren() > 0)
                    registers_[static_cast<size_t> (i)] = snap.getChild (0).createCopy();
            }
        registers_[static_cast<size_t> (active_)] = std::nullopt;             // active is live, never stored
        if (const auto live = t.getChildWithName (kLive); live.isValid() && live.getNumChildren() > 0)
            apply_ (live.getChild (0).createCopy());
    }

    int inferCount (const juce::ValueTree& t) const
    {
        const auto snaps = t.getChildWithName (kSnaps);      // legacy toTree emitted one <Snap> per register
        return snaps.isValid() ? snaps.getNumChildren() : cfg_.numRegisters;
    }

    // Commit any pending burst on the active track as ONE undo step. Returns whether it committed.
    // Suppression DOMINATES every commit path (not just tick): while suppressed, nothing records.
    bool commitPendingBurst()
    {
        if (suppressDepth_ > 0)
            return false;
        Track& tr = activeTrack();
        if (! tr.baseline.isValid())
            return false;
        const juce::ValueTree cur = captureSnapshot();
        if (cur.isEquivalentTo (tr.baseline))
            return false;
        pushUndo (tr, { tr.baseline, {}, Reason::Edit });
        tr.baseline = tr.prev = cur;
        tr.settle = 0;
        ++editSerial_;
        fireHistoryChanged();
        return true;
    }

    // Apply a discrete edit and commit it as ONE undo step immediately (independent of the settle
    // timer): flush any prior burst, snapshot before/after, record the before if it changed.
    template <class MutateFn>
    void commitEdit (Reason reason, const juce::String& label, MutateFn&& mutate)
    {
        commitPendingBurst();
        Track& tr = activeTrack();
        const juce::ValueTree before = captureSnapshot();
        mutate();
        const juce::ValueTree after = captureSnapshot();
        if (after.isEquivalentTo (before))
            return;                                          // genuine no-op edit — fire nothing (B5 matrix)
        tr.baseline = tr.prev = after;                       // keep the baseline current either way
        tr.settle = 0;
        if (suppressDepth_ > 0)
            return;                                          // suppressed: the mutation stands, but record/notify NOTHING
        pushUndo (tr, { before, label, reason });
        ++editSerial_;
        fireAfterApply (reason);
        fireHistoryChanged();
    }

    // Commit the open (outermost) gesture on the active track as one labelled step, if it changed.
    // Suppression dominates: a gesture ending while suppressed records nothing.
    void commitGesture()
    {
        if (suppressDepth_ > 0) { gestureLabel_.clear(); return; }
        Track& tr = activeTrack();
        if (tr.baseline.isValid())
        {
            const juce::ValueTree cur = captureSnapshot();
            if (! cur.isEquivalentTo (tr.baseline))
            {
                pushUndo (tr, { tr.baseline, gestureLabel_, Reason::Gesture });
                tr.baseline = tr.prev = cur;
                tr.settle = 0;
                ++editSerial_;
                fireHistoryChanged();
            }
        }
        gestureLabel_.clear();
    }

    // A history-consuming entry point reached with a gesture still open is consumer misuse (e.g. a
    // register switch mid-drag). Flag it, then force-commit the open gesture on the CURRENT track so
    // its live edits land on the right register rather than corrupting the next one.
    void closeOpenGestureOnMisuse()
    {
        if (gestureDepth_ == 0)
            return;
        jassertfalse;
        gestureDepth_ = 0;
        commitGesture();
    }

    void pushUndo (Track& tr, Entry e)
    {
        tr.undo.push_back (std::move (e));
        trimStack (tr.undo);
        tr.redo.clear();
    }

    void reseed()
    {
        Track& tr = activeTrack();
        tr.baseline = tr.prev = captureSnapshot();
        tr.settle = 0;
    }

    void trimStack (std::vector<Entry>& s) const
    {
        while (static_cast<int> (s.size()) > cfg_.maxUndo)
            s.erase (s.begin());
    }

    void fireAfterApply (Reason r)
    {
       #if JUCE_DEBUG
        const juce::ValueTree beforeCb = capture_();
       #endif
        if (onAfterApply)
            onAfterApply (r);
       #if JUCE_DEBUG
        // Contract 2: onAfterApply must not mutate the live editable state (synchronous writes only).
        jassert (capture_().isEquivalentTo (beforeCb));
       #endif
    }

    void fireHistoryChanged() { if (onHistoryChanged) onHistoryChanged(); }

    static constexpr const char* kWorkspace = "Workspace";
    static constexpr const char* kLive      = "Live";
    static constexpr const char* kSnaps     = "Snaps";
    static constexpr const char* kSnap      = "Snap";
    static constexpr const char* kActive    = "active";
    static constexpr const char* kIndex     = "i";
    static constexpr const char* kSchema    = "schema";
    static constexpr const char* kCount     = "count";
    static constexpr int         kSchemaVersion = 1;

    Mode                                         mode_;
    std::function<juce::ValueTree()>             capture_;
    std::function<void (const juce::ValueTree&)> apply_;
    Config                                       cfg_;
    std::vector<std::optional<juce::ValueTree>>  registers_;   // active slot = nullopt (live is authoritative)
    int                                          active_ = 0;
    std::vector<Track>                           tracks_;      // 1 (WholeWorkspace) or numRegisters (PerRegister)

    int             gestureDepth_  = 0; // refcounted ScopedGesture depth
    int             suppressDepth_ = 0; // refcounted ScopedSuppress depth (dominates gestures)
    juce::String    gestureLabel_;      // label of the outermost open gesture
    juce::ValueTree suppressBaseline_;   // active-live snapshot at outermost beginSuppress (clears the ACTIVE redo)
    juce::ValueTree suppressWsBaseline_; // whole-workspace snapshot at outermost beginSuppress (dirty; catches non-active
                                         // register writes AND is net-zero-correct, unlike a sticky flag)
    bool         inApply_       = false;// re-entrancy guard (ScopedFlag) over every public mutator
    long long    editSerial_    = 0;    // monotonic; advances on every state change
    long long    savedSerial_   = 0;    // editSerial_ at the last markSaved()/load

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompareHistory)
};

} // namespace felitronics::appkit

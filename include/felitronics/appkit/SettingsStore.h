// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::SettingsStore — the family's app-data settings.json: one JSON object under
// userApplicationDataDirectory/<Company>/<Product>/settings.json, read tolerantly, written via a
// verified temp-file swap. Products keep their own keys; this owns only the file discipline.
// Header-only; the consumer supplies JUCE (juce_core / juce_data_structures via var).
//
// WRITE DISCIPLINE (promoted from TabbyEQ's AppPreferences and hardened behind an adversarial
// review — a plugin lives in several host processes at once, and set() is a read-modify-write):
//
//   • TWO LOCK LAYERS, both bounded by kLockTimeoutMs end-to-end: a process-wide mutex first
//     (threads/instances — on POSIX the InterProcessLock below is an fcntl lock, and fcntl is
//     PER-PROCESS: two fds inside one process do not exclude each other; pinned by the gate's
//     two-thread test, which fails on a single-layer version), then a named InterProcessLock
//     (processes). Mutations that cannot take both within the budget FAIL (return false) — an
//     honest failure beats a silent clobber, and an unbounded wait inside a host callback is a
//     hang. load() degrades to a lockless read on timeout (the swap is atomic: old-or-new,
//     never torn).
//   • The lock NAME is derived from the CANONICAL FILE PATH (lower-cased, hashed) — never from
//     the raw company/product strings: "Darwin's Cat" vs "DarwinsCat" must not silently take
//     different locks on the same file, and two different files must not share one lock (test
//     stores in distinct temp dirs included).
//   • save() replaces the WHOLE file from the caller's snapshot — it cannot know the snapshot is
//     fresh, so a concurrent peer's keys can be lost. Prefer set() for one key and apply() for
//     multi-key transactions: both hold the lock across the whole load→mutate→write cycle.
//   • Writes are VERIFIED: JUCE's own replaceWithText ignores a short write (disk-full publishes
//     truncated JSON) — here the temp file's stream status is checked before the swap.
//
// KNOWN RESIDUAL (documented, not solved — same hole exists in today's per-product stores): the
// process mutex is one static per LINKED IMAGE. Two different plugin BUNDLES of the same product
// (AU + VST3) in one host process have separate statics AND per-process fcntl — their concurrent
// RMWs are not excluded. Closing this needs an flock-class in-process-visible lock; tracked as a
// follow-up, do not claim this store covers that case. Values written by another process are
// visible from the next load(); there is no change notification.
//==============================================================================

#include <juce_core/juce_core.h>

#include <functional>

namespace felitronics::appkit
{

class SettingsStore
{
public:
    SettingsStore (juce::String company, juce::String product)
        : company_ (std::move (company)), product_ (std::move (product)) {}

    // Test isolation: redirect the store into an explicit directory (the ctest harness / a
    // consumer's headless tests must never touch the developer's real settings file).
    SettingsStore (juce::String company, juce::String product, juce::File baseDirOverride)
        : company_ (std::move (company)), product_ (std::move (product)),
          baseOverride_ (std::move (baseDirOverride)) {}

    juce::File file() const
    {
        auto base = baseOverride_;
        if (base == juce::File())
        {
            base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
            base = base.getChildFile ("Application Support");   // JUCE's mac value is bare ~/Library
#endif
        }
        return base.getChildFile (company_).getChildFile (product_).getChildFile ("settings.json");
    }

    // The total time a caller may stall on contended locks (both layers share the budget).
    // Bounded ON PURPOSE: an infinite wait inside a host callback is a hang.
    static constexpr int kLockTimeoutMs = 2000;

    // Missing/corrupt file → an empty object (never a void var: callers chain getProperty safely).
    juce::var load() const
    {
        const BoundedLock bl (lock_, kLockTimeoutMs);       // degrade to lockless on timeout: the
        return loadUnlocked();                              // swap is atomic — old-or-new, never torn
    }

    // Whole-file replace from the CALLER's snapshot — see the header note; prefer set()/apply().
    bool save (const juce::var& v) const
    {
        const BoundedLock bl (lock_, kLockTimeoutMs);
        if (! bl.locked) return false;                      // honest failure — never clobber a peer's write
        return saveUnlocked (v);
    }

    // Read-modify-write one key. The WHOLE cycle holds the locks — that is the point of having them.
    bool set (const juce::Identifier& key, const juce::var& value) const
    {
        return apply ([&] (juce::var& v) { v.getDynamicObject()->setProperty (key, value); });
    }

    // Multi-key transaction: load → mutate (your lambda) → verified write, all under the locks.
    bool apply (const std::function<void (juce::var&)>& mutate) const
    {
        const BoundedLock bl (lock_, kLockTimeoutMs);
        if (! bl.locked) return false;
        auto v = loadUnlocked();
        mutate (v);
        return saveUnlocked (v);
    }

    juce::var get (const juce::Identifier& key, const juce::var& fallback = {}) const
    {
        return load().getProperty (key, fallback);
    }

    // The system-wide lock name — derived from the canonical file path (see the header note).
    // NB it names only the CROSS-PROCESS layer: same-process external writers are NOT covered by
    // it (fcntl is per-process) — go through a SettingsStore instance instead.
    juce::String lockName() const
    {
        return "felitronics.settings." +
               juce::String::toHexString (file().getFullPathName().toLowerCase().hashCode64());
    }

private:
    // One mutex per linked image for all SettingsStore instances — layer 1 of the discipline.
    // Settings I/O is rare and tiny, so a single coarse mutex is deliberate.
    static juce::CriticalSection& processMutex()
    {
        static juce::CriticalSection cs;
        return cs;
    }

    // RAII: BOTH layers under ONE deadline — bounded tryEnter spin on the process mutex
    // (a plain ScopedLock would wait forever while a peer holds the fcntl lock, stacking N
    // callers × the fcntl timeout), then the named InterProcessLock with the REMAINING budget.
    // On an fcntl timeout the process mutex is released IMMEDIATELY (crew finding: a degraded
    // load() must not block every writer for the length of its unlocked read). MESSAGE-THREAD
    // callers only — the store does file I/O, it has no business anywhere near the audio thread,
    // locks or no locks. Re-entrancy (set() from inside apply()'s mutate) is supported: JUCE's
    // CriticalSection is documented re-entrant (PTHREAD_MUTEX_RECURSIVE) and InterProcessLock is
    // recursive per object; the two BoundedLocks unwind their own refcounts.
    struct BoundedLock
    {
        BoundedLock (juce::InterProcessLock& l, int ms) : lk (l)
        {
            const auto deadline = juce::Time::getMillisecondCounterHiRes() + ms;
            bool cs = false;
            while (! (cs = processMutex().tryEnter()))
            {
                if (juce::Time::getMillisecondCounterHiRes() >= deadline) return;
                juce::Thread::sleep (1);
            }
            const int remaining = (int) (deadline - juce::Time::getMillisecondCounterHiRes());
            if (! lk.enter (juce::jmax (1, remaining)))
            {
                processMutex().exit();                      // don't hold layer 1 around degraded work
                return;
            }
            csLocked = ipcLocked = locked = true;
        }
        ~BoundedLock()
        {
            if (ipcLocked) lk.exit();
            if (csLocked) processMutex().exit();
        }
        juce::InterProcessLock& lk;
        bool csLocked = false, ipcLocked = false, locked = false;
        JUCE_DECLARE_NON_COPYABLE (BoundedLock)
    };

    juce::var loadUnlocked() const
    {
        const auto f = file();
        if (f.existsAsFile())
            if (auto v = juce::JSON::parse (f.loadFileAsString()); v.getDynamicObject() != nullptr)
                return v;
        return juce::var (new juce::DynamicObject());
    }

    // Verified temp-file swap: JUCE's replaceWithText ignores a short write (disk-full would
    // publish truncated JSON and a later set() would then wipe the rest) — check the stream
    // status BEFORE overwriting the target (crew finding).
    bool saveUnlocked (const juce::var& v) const
    {
        auto f = file();
        if (! f.getParentDirectory().createDirectory()) return false;
        juce::TemporaryFile tmp (f);
        {
            juce::FileOutputStream os (tmp.getFile());
            if (os.failedToOpen()) return false;
            os.writeText (juce::JSON::toString (v), false, false, "\n");
            os.flush();
            if (os.getStatus().failed()) return false;
        }
        return tmp.overwriteTargetFileWithTemporary();
    }

    juce::String company_, product_;
    juce::File baseOverride_;
    // One lock object per store instance; the NAME (path-derived) makes it system-wide. OS
    // resources are taken on enter(), so eager construction stays cheap in host scan paths.
    // Declared LAST — its initializer reads company_/product_/baseOverride_ via file().
    mutable juce::InterProcessLock lock_ { lockName() };
};

} // namespace felitronics::appkit

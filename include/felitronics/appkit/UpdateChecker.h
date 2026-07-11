// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_events/juce_events.h>                    // AsyncUpdater (message-thread delivery)
#include <juce_data_structures/juce_data_structures.h>  // PropertiesFile (pulls juce_core: Thread, URL, WebInputStream, JSON)
#include <felitronics/appkit/UpdateCompare.h>           // the pure, JUCE-free compare rule (ctest-covered)

#include <functional>
#include <utility>                                      // std::exchange

//==============================================================================
// felitronics::appkit::UpdateChecker — the shared, opt-in GitHub-release update check, consolidated
// from the diverged OrbitCab/TabbyEQ copies. Header-only; the consumer supplies JUCE (juce_events +
// juce_data_structures). Lives in felitronics-appkit — the family's APP-SIDE infrastructure repo —
// so felitronics-core stays conceptually pure DSP with zero JUCE anywhere in its tree.
//
// Policy (strict, product-blind): the network is touched ONLY when checkNow() is called from a user
// click — NEVER on launch, NEVER silently, NEVER on the audio thread. The product reads the latest
// release tag and decides "outdated" itself (UpdateCompare.h), then persists the newer tag in the
// caller-supplied global PropertiesFile so the "update available" badge survives restarts and is
// shared across instances.
//
// Contract:
//   GET https://api.github.com/repos/<Config::ownerRepo>/releases/latest
//   200 -> { tag_name: "vX.Y.Z", html_url, name, … } — we read tag_name (strip 'v'), REQUIRE it to
//          be a clean release tag (see fetch()), compare it to Config::currentVersion
//          (UpdateCompare), and expose html_url as the release page (falling back to the canonical
//          github.com/<slug>/releases/latest so the UI never gets a dead end).
//   404 = no releases yet, 403 = rate-limited; any non-200 / timeout / no network / unusable tag
//   => silent failure (ok=false) — this is an opt-in, non-blocking check.
//
// Threading: the worker is an OWNED juce::Thread base, so destruction JOINS it —
// signalThreadShouldExit() + WebInputStream::cancel() abort a blocking connect/read promptly, then
// stopThread() waits. That is the crash-safe replacement for a detached juce::Thread::launch
// reaching back via a cross-thread WeakReference, which could keep executing after the host
// unloaded the plugin module. The result is handed to the message thread via AsyncUpdater
// (cancelPendingUpdate() in the destructor drops an undelivered one). Hosts destroy the processor
// on the message thread (JUCE wrapper guarantee), so a delivered callback can't race destruction.
//
// Left non-final on purpose: consumers subclass it as a thin adapter that bakes in their Config.
//==============================================================================
namespace felitronics::appkit
{

class UpdateChecker : private juce::Thread,
                      private juce::AsyncUpdater
{
public:
    //==========================================================================
    // Everything product-specific lives here; the checker itself is product-blind.
    struct Config
    {
        // GitHub "<owner>/<repo>" slug, e.g. "darwinscat/orbitcab". Drives both the API URL and the
        // canonical releases page. REQUIRED.
        juce::String ownerRepo;

        // Display name, e.g. "TabbyEQ" — the worker-thread name and the User-Agent token (spaces
        // stripped there: a UA product token can't contain them, and GitHub 403s UA-less requests).
        juce::String productName;

        // The running build's version: a clean "X.Y.Z"/"vX.Y.Z" (JucePlugin_VersionString) or a
        // `git describe` stamp ("v0.1.0-78-g…", "-dirty", "unknown"). UpdateCompare.h handles both —
        // anything that isn't a clean release tag counts OLDER than any release (dev rule).
        juce::String currentVersion;

        // Access to the product's global PropertiesFile (per-machine badge persistence, shared
        // across instances — typically `[&prefs] { return prefs.file(); }`). The function and its
        // result may be null — then the badge never persists but checkNow() still works (handy for
        // tests). Called on the MESSAGE THREAD only (ctor / handleAsyncUpdate / badge getters) —
        // PropertiesFile is not thread-safe, which is exactly why persistence happens in
        // handleAsyncUpdate and never on the worker. The captured object must outlive this checker —
        // in a consumer, declare the prefs owner BEFORE the checker member (destroyed after it).
        std::function<juce::PropertiesFile*()> settings;

        // Settings keys for the persisted badge. Namespaced defaults — the file is an app-wide
        // store, so bare names invite collisions. A product with historical bare keys (OrbitCab)
        // overrides these so its users' stored badges survive the migration.
        juce::String keyLatest = "updateLastSeenLatest";
        juce::String keyEpoch  = "updateLastCheckEpoch";

        // Connect timeout — short: opt-in, non-blocking. The body-read budget is derived as 2× this
        // (see fetch()) and the destructor join budget as this + 3 s: cancel() aborts a blocked
        // connect/read promptly, so that is generous headroom before stopThread force-kills (last
        // resort). Clamped to [1000, 30000] in the ctor — 0/negative would collapse the read
        // deadline to "now" and break the join budget (crew finding).
        int connectTimeoutMs = 5000;
    };

    struct Result
    {
        bool         ok       = false;   // got a usable 200 response with a clean release tag
        bool         outdated = false;   // a newer release exists than the running build
        juce::String latest;             // the remote tag, 'v' stripped (e.g. "0.2.0")
        juce::String url;                // the release page (where to send the user; never empty when ok)
        juce::String notes;              // optional human note (release title; empty if absent)
    };

    explicit UpdateChecker (Config cfg)
        : juce::Thread (cfg.productName + " UpdateChecker"),
          config (std::move (cfg))
    {
        jassert (config.ownerRepo.isNotEmpty());    // no slug, no checker

        // See the Config field docs: unclamped 0/negative would zero the read deadline and the join budget.
        config.connectTimeoutMs = juce::jlimit (1000, 30000, config.connectTimeoutMs);

        // Cross-process freshness: same-process instances share the PropertiesFile object (the
        // product's prefs owner uses a SharedResourcePointer), but another HOST PROCESS may have
        // stored a newer tag — pick it up once here.
        if (auto* s = store())
            s->reload();

        // Badge-clear: if the running build has caught up to (or passed) a previously seen
        // "latest", drop the stored value so no stale badge shows. The ONLY ctor-time compare.
        const juce::String seen = storedLatest();
        if (seen.isNotEmpty() && ! update::remoteIsNewer (config.currentVersion.toStdString(), seen.toStdString()))
            clearStored();
    }

    ~UpdateChecker() override
    {
        // JOIN the worker before the plugin module can unload: signal, abort any blocking
        // connect/read, wait. Then drop an undelivered async result (the AsyncUpdater base asserts
        // on pending updates).
        signalThreadShouldExit();
        {
            const juce::ScopedLock sl (streamLock);
            if (activeStream != nullptr)
                activeStream->cancel();             // cross-thread-safe: unblocks connect/read promptly
        }
        stopThread (config.connectTimeoutMs + 3000);
        cancelPendingUpdate();
    }

    juce::String currentVersion() const { return config.currentVersion; }

    // Fire the check on the owned background worker; `onDone` is invoked on the message thread with
    // the result (ok=false on any failure — silent, opt-in). MESSAGE THREAD ONLY. If a check is
    // already in flight, the new callback replaces the pending one (last click wins) — the
    // in-flight result is delivered to it.
    void checkNow (std::function<void (Result)> cb)
    {
        JUCE_ASSERT_MESSAGE_THREAD
        onDone = std::move (cb);
        // Also gate on isUpdatePending(): between run() calling triggerAsyncUpdate() and
        // handleAsyncUpdate() actually firing, isThreadRunning() is already false while the result
        // is still queued. Without this a second click there would spin up a 2nd worker that races
        // `pending`; instead we keep the replaced callback and deliver the in-flight result to it
        // (last click wins).
        if (! isThreadRunning() && ! isUpdatePending())
            if (! startThread())                    // thread creation failed (exotic) — don't strand the click:
                if (auto cb2 = std::exchange (onDone, nullptr))
                    cb2 ({});                       // deliver the silent-failure contract (ok=false) synchronously
    }

    // Badge state: a stored "latest" tag that is newer than the running build.
    bool updateAvailable() const
    {
        const juce::String seen = storedLatest();
        return seen.isNotEmpty() && update::remoteIsNewer (config.currentVersion.toStdString(), seen.toStdString());
    }

    juce::String storedLatest() const               // the bare stored tag ('v' stripped), or empty
    {
        if (auto* s = store())
            return s->getValue (config.keyLatest);
        return {};
    }

    // The canonical release-page URL — the badge's click-through when a stored tag exists but no
    // html_url was saved, and fetch()'s fallback when the response lacks one.
    juce::String releasesPageUrl() const { return "https://github.com/" + config.ownerRepo + "/releases/latest"; }

private:
    juce::PropertiesFile* store() const { return config.settings ? config.settings() : nullptr; }

    void storeOutdated (const juce::String& latest)
    {
        if (auto* s = store())
        {
            s->setValue (config.keyLatest, latest);
            // keyEpoch is written for support/diagnostics ("when did this box last see GitHub?") —
            // nothing in the checker reads it back; both originals recorded it, kept for parity.
            s->setValue (config.keyEpoch, juce::String (juce::Time::getCurrentTime().toMilliseconds()));
            s->saveIfNeeded();
        }
    }

    void clearStored()
    {
        if (auto* s = store())
        {
            s->removeValue (config.keyLatest);
            s->saveIfNeeded();
        }
    }

    // A remote tag ("v0.2.0"/"0.2.0") is stored/compared bare: strip a single leading 'v'.
    static juce::String stripV (juce::String tag)
    {
        tag = tag.trim();
        return tag.startsWithIgnoreCase ("v") ? tag.substring (1) : tag;
    }

    // Worker thread. Blocking; the stream is registered under streamLock while it can block so the
    // destructor can cancel() it from another thread. Reads `config` freely — it is immutable after
    // construction.
    Result fetch()
    {
        Result r;

        // GitHub's API rejects requests without a User-Agent (403); Accept + api-version are best
        // practice. Unauthenticated → 60 req/h per IP, ample for an opt-in manual check.
        const juce::String headers =
            "User-Agent: " + config.productName.removeCharacters (" ") + "-UpdateChecker\r\n"
            "Accept: application/vnd.github+json\r\n"
            "X-GitHub-Api-Version: 2022-11-28";

        juce::WebInputStream web (juce::URL ("https://api.github.com/repos/" + config.ownerRepo + "/releases/latest"),
                                  false);
        web.withConnectionTimeout (config.connectTimeoutMs)
           .withNumRedirectsToFollow (3)
           .withExtraHeaders (headers);

        {
            const juce::ScopedLock sl (streamLock);
            if (threadShouldExit())
                return r;                           // shutting down — never even start the request
            activeStream = &web;                    // publish for cross-thread cancel()
        }

        const bool connected = web.connect (nullptr);   // no network / DNS / TLS failure → false

        // Read the body in BOUNDED chunks: a hard byte cap (a /releases/latest payload is a few KB;
        // >1 MiB means a captive portal or a hostile endpoint — don't buffer it) and a wall-clock
        // deadline (on curl backends the connect timeout doesn't bound a post-connect trickle; a
        // slow-drip server must not stall the worker until destruction). Chunks are SMALL (1 KiB)
        // on purpose: read() may block until it fills the requested count, so the chunk size is
        // the granularity at which the deadline and shutdown can interrupt a trickling body (crew
        // finding). readEntireStreamAsString bounds none of these; the destructor's cancel()
        // remains the hard backstop.
        juce::MemoryBlock raw;
        if (connected && web.getStatusCode() == 200)    // 404 = no releases yet, 403 = rate-limited, etc.
        {
            constexpr size_t kMaxBodyBytes = 1 << 20, kChunkBytes = 1024;
            const double deadline = juce::Time::getMillisecondCounterHiRes() + 2.0 * config.connectTimeoutMs;
            juce::HeapBlock<char> chunk (kChunkBytes);
            while (raw.getSize() < kMaxBodyBytes && ! threadShouldExit()
                   && juce::Time::getMillisecondCounterHiRes() < deadline)
            {
                const int n = web.read (chunk, (int) juce::jmin (kChunkBytes, kMaxBodyBytes - raw.getSize()));
                if (n <= 0)
                    break;                          // EOF, error, or cancelled
                raw.append (chunk, (size_t) n);
            }
        }

        {
            const juce::ScopedLock sl (streamLock);
            activeStream = nullptr;                 // retire BEFORE the stack-scope stream dies
        }

        if (threadShouldExit() || raw.getSize() == 0)
            return r;

        // A cap/deadline-truncated body can end mid-multibyte-character; String::fromUTF8 of an
        // invalid sequence trips a debug assertion inside JUCE (crew finding). Validate first —
        // invalid UTF-8 is just another silent failure.
        const auto* data = static_cast<const char*> (raw.getData());
        if (! juce::CharPointer_UTF8::isValidString (data, (int) raw.getSize()))
            return r;

        const juce::var json = juce::JSON::parse (juce::String::fromUTF8 (data, (int) raw.getSize()));
        if (! json.isObject())
            return r;                               // includes most truncated bodies — silent fail

        // /releases/latest excludes drafts and flagged pre-releases, but the tag itself is
        // whatever the repo owner typed. REQUIRE a clean release tag (vX.Y.Z): an exotic tag
        // ("release-2026-07", "v1.2.4-rc1") must be a silent failure — reporting it as ok would
        // make handleAsyncUpdate's clearStored() wipe a previously stored REAL badge on the
        // strength of an unusable tag (crew finding). Stored/compared bare ('v' stripped).
        const juce::String tag = stripV (json.getProperty ("tag_name", juce::var()).toString());
        if (tag.isEmpty() || ! update::isCleanRelease (tag.toStdString()))
            return r;

        r.ok       = true;
        r.latest   = tag;
        r.url      = json.getProperty ("html_url", juce::var()).toString();   // the release page
        r.notes    = json.getProperty ("name",     juce::var()).toString();   // release title (optional)
        if (r.url.isEmpty())
            r.url = releasesPageUrl();              // never hand the UI a dead end on a good response
        r.outdated = update::remoteIsNewer (config.currentVersion.toStdString(), tag.toStdString());
        return r;
    }

    void run() override
    {
        const Result r = fetch();
        if (threadShouldExit())
            return;                                 // shutting down — drop the result silently
        pending = r;                                // visible to handleAsyncUpdate: the message post synchronizes
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        // Message thread. Persist first (PropertiesFile is not thread-safe), then notify the UI.
        // A successful check is the freshest truth: write the badge up when a newer release exists,
        // and clear it when it doesn't — otherwise a stored "latest" the server later retracts (a
        // yanked release) would nag forever, since only a version bump clears it (ctor). A FAILED
        // check (ok=false) touches nothing, so an offline click never wipes a real badge.
        if (pending.ok && pending.outdated && pending.latest.isNotEmpty())
            storeOutdated (pending.latest);
        else if (pending.ok)
            clearStored();

        if (auto cb = std::exchange (onDone, nullptr))
            cb (pending);
    }

    Config config;                                  // immutable after construction → worker may read it

    std::function<void (Result)> onDone;            // message thread only (checkNow / handleAsyncUpdate)
    Result pending;                                 // written by the worker before triggerAsyncUpdate

    // The in-flight stream, registered so the destructor can abort a blocking connect/read from
    // another thread (WebInputStream::cancel is documented cross-thread-safe). Guarded by
    // streamLock: the worker only publishes/retires the pointer under the lock, so cancel() can
    // never race the stream's stack-scope destruction.
    juce::CriticalSection streamLock;
    juce::WebInputStream* activeStream = nullptr;   // guarded by streamLock

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateChecker)
};

} // namespace felitronics::appkit

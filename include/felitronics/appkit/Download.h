// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::download — a BLOCKING, verify-then-commit file fetch for worker threads
// (managed-runtime bootstraps, asset mirrors): stream to `<dest>.part`, optionally check sha256,
// then rename — a crashed/failed download never leaves a plausible-looking file behind. Progress
// lands on the CALLING thread; marshal to the message thread yourself if you paint with it.
// NEVER call on the message thread or the audio thread. Header-only; the consumer supplies JUCE
// (juce_core + juce_cryptography).
//==============================================================================

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

#include <functional>

namespace felitronics::appkit
{

struct DownloadResult
{
    bool ok = false;
    juce::String error;         // empty when ok
};

// expectedSha256Hex: empty = skip verification. onProgress(bytesSoFar, totalBytesOrMinus1) —
// return false from it to abort (the .part is removed).
inline DownloadResult downloadFile (const juce::URL& url, const juce::File& dest,
                                    const juce::String& expectedSha256Hex = {},
                                    const std::function<bool (juce::int64, juce::int64)>& onProgress = {})
{
    DownloadResult r;
    auto part = dest.getSiblingFile (dest.getFileName() + ".part");
    part.deleteFile();
    if (! part.getParentDirectory().createDirectory()) { r.error = "cannot create " + dest.getParentDirectory().getFullPathName(); return r; }

    auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                         .withConnectionTimeoutMs (15000)
                                         .withNumRedirectsToFollow (8));
    if (in == nullptr) { r.error = "connection failed: " + url.toString (false); return r; }

    const juce::int64 total = in->getTotalLength();
    {
        juce::FileOutputStream os (part);
        if (! os.openedOk()) { r.error = "cannot write " + part.getFullPathName(); return r; }
        juce::HeapBlock<char> buf (1 << 16);
        juce::int64 done = 0;
        while (! in->isExhausted())
        {
            const int n = in->read (buf.getData(), 1 << 16);
            if (n < 0) { part.deleteFile(); r.error = "read failed mid-stream"; return r; }
            if (n == 0) break;
            if (! os.write (buf.getData(), (size_t) n)) { part.deleteFile(); r.error = "disk write failed"; return r; }
            done += n;
            if (onProgress && ! onProgress (done, total)) { part.deleteFile(); r.error = "cancelled"; return r; }
        }
        os.flush();
    }

    if (expectedSha256Hex.isNotEmpty())
    {
        juce::FileInputStream fis (part);
        if (! fis.openedOk()) { part.deleteFile(); r.error = "cannot re-read for verification"; return r; }
        const auto got = juce::SHA256 (fis).toHexString();
        if (! got.equalsIgnoreCase (expectedSha256Hex))
        {
            part.deleteFile();
            r.error = "sha256 mismatch (got " + got.substring (0, 12) + "…, wanted "
                    + expectedSha256Hex.substring (0, 12) + "…)";
            return r;
        }
    }

    dest.deleteFile();
    if (! part.moveFileTo (dest)) { part.deleteFile(); r.error = "final rename failed"; return r; }
    r.ok = true;
    return r;
}

} // namespace felitronics::appkit

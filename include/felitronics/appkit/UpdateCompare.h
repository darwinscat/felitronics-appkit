// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <string_view>

//==============================================================================
// felitronics::appkit::update — the update-check comparison rule: a PURE, header-only, JUCE-FREE
// function, so this repo's own ctest can cover it without JUCE (unlike UpdateChecker.h next door,
// which is compiled against the consumer's JUCE). It answers one question: given the running
// build's version string and the latest GitHub release tag, should the "update available" badge
// light?
//
// The local version comes in two shapes across the product family, and one rule handles both:
//   • A CLEAN SEMVER — JucePlugin_VersionString ("1.1.0") or a clean-tag CI stamp ("vX.Y.Z", the
//     tree is clean so `git describe` emits the bare tag). These compare numerically.
//   • A `git describe --tags --always --dirty` stamp — "v0.1.0-78-gHASH" (commits past the tag),
//     a "-dirty" suffix, "0.0.0-dev", "unknown". By decision, such a DEV build counts OLDER than
//     ANY release, so the badge lights whenever a real release exists. (Consequence: a developer
//     on a between-releases build always sees the badge once a release is published — intended:
//     "you are not on a released build.")
//
// End users only ever run clean-tag CI builds, so they get correct numeric semver behaviour.
namespace felitronics::appkit::update
{
    struct Semver { int major = 0, minor = 0, patch = 0; };

    // Hostile-input bound: a version component may carry at most 6 digits (max 999999 — far beyond
    // any real release). More digits marks the whole string UNPARSEABLE ({0,0,0} / not clean), so a
    // hostile tag can never sign-overflow the int accumulation (999999 * 10 + 9 << INT_MAX).
    inline constexpr int kMaxComponentDigits = 6;

    // Parse major.minor.patch from the LEADING part of a version string. A single leading 'v'/'V'
    // is stripped; scanning stops at the first char that is neither a digit nor a '.', so
    // "v0.1.0-78-g7ba0138" -> {0,1,0}, "0.0.0-dev" -> {0,0,0}, "unknown" -> {0,0,0}. Missing
    // components default to 0. Any component longer than kMaxComponentDigits rejects the whole
    // string to {0,0,0} (see the bound above).
    constexpr Semver parseSemver (std::string_view s) noexcept
    {
        if (! s.empty() && (s.front() == 'v' || s.front() == 'V'))
            s.remove_prefix (1);

        Semver out {};
        int idx = 0, val = 0, digits = 0;                       // idx: 0=major 1=minor 2=patch
        auto commit = [&] { if (idx == 0) out.major = val; else if (idx == 1) out.minor = val; else if (idx == 2) out.patch = val; };
        for (char c : s)
        {
            if (c >= '0' && c <= '9')
            {
                if (++digits > kMaxComponentDigits) return {};  // hostile/absurd component → unparseable
                val = val * 10 + (c - '0');
            }
            else if (c == '.') { commit(); if (++idx > 2) return out; val = 0; digits = 0; }
            else               break;                           // '-', '+', letter, space … : end of the numeric version
        }
        commit();
        return out;
    }

    // Is `describe` an exact, clean release tag "vX.Y.Z" (nothing after the patch)? Any commits-ahead
    // suffix ("-N-g…"), "-dirty", pre-release text, "0.0.0-dev" or "unknown" makes it NOT clean → a dev
    // build. An over-long component (see kMaxComponentDigits) is also not clean, keeping this
    // consistent with parseSemver's rejection.
    constexpr bool isCleanRelease (std::string_view s) noexcept
    {
        if (! s.empty() && (s.front() == 'v' || s.front() == 'V'))
            s.remove_prefix (1);
        if (s.empty()) return false;

        int dots = 0, digits = 0;
        bool digitInPart = false;
        for (char c : s)
        {
            if      (c >= '0' && c <= '9') { digitInPart = true; if (++digits > kMaxComponentDigits) return false; }
            else if (c == '.') { if (! digitInPart) return false; ++dots; digitInPart = false; digits = 0; }
            else               return false;                    // any '-', letter, etc. → not a clean release
        }
        return dots == 2 && digitInPart;                        // exactly major.minor.patch, patch present
    }

    // Should the badge light? True iff `remoteTag` is a real release that is NEWER than the build
    // identified by `localDescribe`. See the file header for the dev-build rule.
    constexpr bool remoteIsNewer (std::string_view localDescribe, std::string_view remoteTag) noexcept
    {
        if (! isCleanRelease (remoteTag))
            return false;                                       // remote pre-release/exotic tags offer nothing

        const Semver r = parseSemver (remoteTag);
        if (r.major == 0 && r.minor == 0 && r.patch == 0)
            return false;                                       // no usable release tag → nothing to offer

        if (! isCleanRelease (localDescribe))
            return true;                                        // dev build: older than ANY release

        const Semver l = parseSemver (localDescribe);
        if (r.major != l.major) return r.major > l.major;
        if (r.minor != l.minor) return r.minor > l.minor;
        return r.patch > l.patch;
    }

    //==========================================================================
    // ONE REPOSITORY, SEVERAL PRODUCTS. A repository that releases more than one product tells
    // them apart by a prefix on the tag — "catpit-v0.1.0" beside the plain "v0.8.0". GitHub's
    // "latest release" is then whichever product shipped last, which is no answer to "is there a
    // newer build of MINE". The two below answer that from the LIST of releases instead.

    // The version part of `tag` when the tag is this product's — the prefix, then a clean release
    // ("catpit-" + "v0.1.0") — and empty otherwise. The prefix must be followed by something: a
    // tag that IS the prefix is nobody's release. An empty prefix accepts any clean release tag.
    constexpr std::string_view versionOfTag (std::string_view tag, std::string_view prefix) noexcept
    {
        if (tag.size() <= prefix.size() || tag.substr (0, prefix.size()) != prefix)
            return {};
        const std::string_view rest = tag.substr (prefix.size());
        return isCleanRelease (rest) ? rest : std::string_view {};
    }

    // A release as the list gives it, reduced to what choosing one needs. `published` is false for
    // a draft or a flagged pre-release — neither is an update to offer.
    struct Listed { std::string_view tag; bool published = true; };

    // The index of the HIGHEST of this product's published releases in `listed`, or -1 when it has
    // none there. Highest by version, not first in the list: the list is in order of creation, and
    // a patch to an older line is created after a newer one.
    template <typename Range>
    constexpr int highestOf (const Range& listed, std::string_view prefix) noexcept
    {
        int best = -1, index = 0;
        std::string_view bestVersion;
        for (const auto& entry : listed)
        {
            const std::string_view version = entry.published ? versionOfTag (entry.tag, prefix) : std::string_view {};
            const Semver v = parseSemver (version);
            const bool usable = ! version.empty() && (v.major != 0 || v.minor != 0 || v.patch != 0);
            if (usable && (best < 0 || remoteIsNewer (bestVersion, version)))
            {
                best = index;
                bestVersion = version;
            }
            ++index;
        }
        return best;
    }
}

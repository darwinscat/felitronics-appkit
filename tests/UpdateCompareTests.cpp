// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// JUCE-free unit for the update-check comparison rule (UpdateCompare.h) — the half of appkit this
// repo's own ctest can cover without JUCE (UpdateChecker.h is compile-gated by ConsumerCompileCheck
// and behaviour-covered by the consumers). Ported from TabbyEQ's tests/update_compare.cpp when the
// rule was promoted here. Locks the documented behaviour: clean-release builds compare by numeric
// semver; a dev build (untagged, commits-ahead, or dirty) counts OLDER than any release; a
// missing/zero/hostile remote tag offers nothing; over-long components reject rather than overflow.

#include <felitronics/appkit/UpdateCompare.h>

#include <cstdio>
#include <string>
#include <string_view>

using namespace felitronics::appkit::update;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

// remoteIsNewer(local, remote) == expected
static void expectNewer (std::string_view local, std::string_view remote, bool expected, const std::string& what)
{
    ok (remoteIsNewer (local, remote) == expected, what);
}

int main()
{
    std::printf ("felitronics::appkit::update tests\n");

    group ("parseSemver: leading 'v', suffix truncation, missing parts");
    ok (parseSemver ("v0.1.0").major == 0 && parseSemver ("v0.1.0").minor == 1 && parseSemver ("v0.1.0").patch == 0, "parse v0.1.0");
    ok (parseSemver ("1.2.3").major == 1 && parseSemver ("1.2.3").minor == 2 && parseSemver ("1.2.3").patch == 3, "parse 1.2.3");
    ok (parseSemver ("v2.0.0-rc1").major == 2 && parseSemver ("v2.0.0-rc1").patch == 0, "parse stops at pre-release suffix");
    ok (parseSemver ("v0.1.0-78-g7ba0138").minor == 1 && parseSemver ("v0.1.0-78-g7ba0138").patch == 0, "parse ignores commits-ahead suffix");
    ok (parseSemver ("0.0.0-dev").major == 0 && parseSemver ("0.0.0-dev").patch == 0, "parse 0.0.0-dev -> zero");
    ok (parseSemver ("unknown").major == 0, "parse unknown -> zero");
    ok (parseSemver ("v3").major == 3 && parseSemver ("v3").minor == 0, "parse partial v3 -> {3,0,0}");
    ok (parseSemver ("v10.20.30").major == 10 && parseSemver ("v10.20.30").minor == 20 && parseSemver ("v10.20.30").patch == 30, "parse multi-digit");

    group ("parseSemver: hostile-input bound (kMaxComponentDigits) — no int overflow");
    ok (parseSemver ("v999999.1.2").major == 999999 && parseSemver ("v999999.1.2").patch == 2, "parse 6-digit component (at the bound)");
    ok (parseSemver ("v1234567.0.0").major == 0 && parseSemver ("v1234567.0.0").minor == 0, "parse 7-digit component -> unparseable {0,0,0}");
    ok (parseSemver ("v1.99999999999999999999.1").major == 0, "parse absurd middle component -> unparseable {0,0,0}");

    group ("isCleanRelease: only a bare vX.Y.Z is a clean release");
    ok (  isCleanRelease ("v0.1.0"),              "v0.1.0 is a clean release");
    ok (  isCleanRelease ("1.2.3"),               "1.2.3 is a clean release");
    ok (! isCleanRelease ("v0.1.0-78-g7ba0138"),  "commits-ahead is NOT clean");
    ok (! isCleanRelease ("v0.1.0-dirty"),        "dirty is NOT clean");
    ok (! isCleanRelease ("0.0.0-dev"),           "0.0.0-dev is NOT clean");
    ok (! isCleanRelease ("unknown"),             "unknown is NOT clean");
    ok (! isCleanRelease ("v0.1"),                "v0.1 (missing patch) is NOT clean");
    ok (! isCleanRelease (""),                    "empty is NOT clean");
    ok (! isCleanRelease ("v1.2.3.4"),            "four-part is NOT clean");
    ok (  isCleanRelease ("v999999.0.0"),         "6-digit component (at the bound) IS clean");
    ok (! isCleanRelease ("v1234567.0.0"),        "7-digit component is NOT clean (matches parse rejection)");

    group ("isCleanRelease: the remote-tag gate (UpdateChecker refuses exotic release tags)");
    ok (! isCleanRelease ("release-2026-07"),     "date-style tag is NOT usable (would have wiped a stored badge)");
    ok (! isCleanRelease ("v1.2.4-rc1"),          "rc tag is NOT usable (must not light the badge)");
    ok (! isCleanRelease ("vv2.0.0"),             "double-v tag is NOT usable");

    group ("remoteIsNewer: clean-release numeric semver compare");
    expectNewer ("v0.1.0", "0.2.0", true,  "clean 0.1.0 < release 0.2.0");
    expectNewer ("v0.2.0", "0.1.0", false, "clean 0.2.0 > release 0.1.0");
    expectNewer ("v0.1.0", "0.1.0", false, "clean 0.1.0 == release 0.1.0 (up to date)");
    expectNewer ("v1.0.0", "0.9.9", false, "major beats minor/patch");
    expectNewer ("v0.9.9", "1.0.0", true,  "release major bump");
    expectNewer ("v0.1.0", "v0.1.1", true, "remote tag may carry a leading v");
    expectNewer ("0.1.0",  "0.1.1", true,  "patch bump");

    group ("remoteIsNewer: a DEV build counts OLDER than ANY release");
    expectNewer ("v0.1.0-78-g7ba0138", "0.1.0", true, "dev (commits-ahead) < same-base release");
    expectNewer ("v0.1.0-78-g7ba0138", "0.9.0", true, "dev < newer release");
    expectNewer ("0.0.0-dev",          "0.1.0", true, "untagged dev < any release");
    expectNewer ("unknown",            "0.1.0", true, "unknown build < any release");
    expectNewer ("v0.2.0-dirty",       "0.1.0", true, "dirty build < an OLDER-numbered release (still dev)");

    group ("remoteIsNewer: no usable remote tag => never newer");
    expectNewer ("v0.1.0",             "",         false, "empty remote tag offers nothing");
    expectNewer ("v0.1.0",             "0.0.0",    false, "0.0.0 remote offers nothing");
    expectNewer ("0.0.0-dev",          "unknown",  false, "unparseable remote offers nothing even to a dev build");
    expectNewer ("v0.1.0", "99999999999999999999.0.0", false, "hostile over-long remote is rejected (no overflow)");
    expectNewer ("v1234567.0.0",       "0.1.0",    true,  "hostile over-long local counts as dev < any release");

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

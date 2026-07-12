// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

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

static void expectNewer (std::string_view local, std::string_view remote, bool expected, const std::string& what)
{
    ok (remoteIsNewer (local, remote) == expected, what);
}

int main()
{
    std::printf ("felitronics::appkit update-compare falsification tests\n");

    group ("remoteIsNewer rejects parseable prefixes that are not clean release tags");
    expectNewer ("1.2.2",     "1.2.3-rc1",    false, "remote rc tag is not a release offer");
    expectNewer ("1.2.2",     "1.2.3+build5", false, "remote build-metadata tag is not a release offer");
    expectNewer ("1.2.2",     "1.2.3.4",      false, "remote four-part tag is not truncated into a release");
    expectNewer ("1.1.9",     "1.2",          false, "remote missing patch is not a release offer");
    expectNewer ("1.2.2",     "1.2.3abc",     false, "remote numeric prefix plus letters is not a release offer");
    expectNewer ("0.0.0-dev", "1.2.3-rc1",    false, "dev builds still ignore unusable remote tags");

    group ("remote clean tags still compare normally after the stricter gate");
    expectNewer ("1.2.2", "1.2.3",     true,  "bare clean remote patch bump");
    expectNewer ("1.2.2", "v1.2.3",    true,  "v-prefixed clean remote patch bump");
    expectNewer ("1.2.3", "V1.2.3",    false, "upper-case V clean remote equal version");
    expectNewer ("1.2.3", "01.002.004", true, "leading-zero clean remote still compares numerically");

    group ("local pathologies keep the documented dev-build semantics");
    expectNewer ("1.2.3-rc1", "1.2.3", true, "local rc build is a dev build older than the release");
    expectNewer ("1.2.3.4",   "1.2.3", true, "local exotic clean-prefix build is still dev older than a release");

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <felitronics/appkit/UpdateCompare.h>

#include <functional>
#include <string>
#include <utility>

#if defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include <felitronics/appkit/UpdateChecker.h>
#undef private
#if defined (__clang__)
 #pragma clang diagnostic pop
#endif

#include <cstdio>

using felitronics::appkit::UpdateChecker;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

static void expectStrip (const juce::String& input, const juce::String& expected, bool cleanAfter, const std::string& what)
{
    const juce::String stripped = UpdateChecker::stripV (input);
    ok (stripped == expected, what + " strips to '" + stripped.toStdString() + "'");
    ok (felitronics::appkit::update::isCleanRelease (stripped.toStdString()) == cleanAfter,
        what + " clean-release gate after strip");
}

int main()
{
    std::printf ("felitronics::appkit update-checker falsification tests\n");

    group ("remote tag normalization strips v only after the whole tag is clean");
    expectStrip ("v1.2.3",       "1.2.3",       true,  "single-v clean tag");
    expectStrip ("V1.2.3",       "1.2.3",       true,  "single-V clean tag");
    expectStrip ("1.2.3",        "1.2.3",       true,  "bare clean tag");
    expectStrip ("vv1.2.3",      "vv1.2.3",     false, "double-v tag remains rejectable");
    expectStrip ("v1.2.3-rc1",   "v1.2.3-rc1",  false, "pre-release tag remains rejectable");
    expectStrip ("version-2026", "version-2026", false, "word tag remains rejectable");

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

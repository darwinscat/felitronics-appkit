// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// Behaviour unit for the device-spec data model (DeviceSpec.h) — ported from OrbitCab's
// tests/NamCodecTests.cpp device-spec group when the model was promoted here, plus edge cases.
// DeviceSpec.h parses with juce::String, so unlike UpdateCompareTests this executable links a real
// juce_core and therefore registers in the FELITRONICS_APPKIT_TESTS_WITH_JUCE tier. Locks the
// documented behaviour: aliases fold case + whitespace; a bare type means count 1; unknown types
// and non-positive counts are DROPPED (a malformed spec yields fewer entries, never garbage);
// per-entry and total glyph counts clamp at kMaxDeviceGlyphs.

#include <felitronics/appkit/DeviceSpec.h>

#include <cstdio>
#include <string>

using namespace felitronics::appkit;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

// parseDeviceSpec(s) == expected
static void expectSpec (const juce::String& s, const DeviceSpec& expected, const std::string& what)
{
    ok (parseDeviceSpec (s) == expected, what);
}

int main()
{
    std::printf ("felitronics::appkit device-spec tests\n");

    group ("deviceFromString: the closed vocabulary, one spelling each");
    ok (deviceFromString ("tube")  == DeviceType::tube,  "tube");
    ok (deviceFromString ("bjt")   == DeviceType::bjt,   "bjt");
    ok (deviceFromString ("fet")   == DeviceType::fet,   "fet");
    ok (deviceFromString ("ic")    == DeviceType::ic,    "ic");
    ok (deviceFromString ("dsp")   == DeviceType::dsp,   "dsp");
    ok (deviceFromString ("diode") == DeviceType::diode, "diode");
    // An analogue op-amp is NOT a DSP: a Tube Screamer's 4558 clips IN the audio path, a Boss GT
    // converts, computes and converts back. They share a package and nothing else.
    ok (deviceFromString ("ic")    != DeviceType::dsp,   "ic is not an alias for dsp any more");

    group ("deviceFromString: pnp/npn still read as bjt (captures on disk are stamped 'pnp')");
    ok (deviceFromString ("pnp") == DeviceType::bjt, "pnp -> bjt");
    ok (deviceFromString ("npn") == DeviceType::bjt, "npn -> bjt");

    group ("deviceFromString: the dropped aliases now read as unknown");
    for (const char* dead : { "valve", "transistor", "jfet", "mosfet", "chip", "digital", "opamp", "op-amp" })
        ok (deviceFromString (dead) == DeviceType::none, std::string (dead) + " -> none");

    group ("deviceFromString: case + whitespace fold; unknown -> none");
    ok (deviceFromString ("BJT")  == DeviceType::bjt,  "upper-case BJT");
    ok (deviceFromString ("TuBe") == DeviceType::tube, "mixed-case TuBe");
    ok (deviceFromString ("  tube  ") == DeviceType::tube, "surrounding whitespace trimmed");
    ok (deviceFromString ("")      == DeviceType::none, "empty -> none");
    ok (deviceFromString ("bogus") == DeviceType::none, "unknown -> none");
    ok (deviceFromString ("tubes") == DeviceType::none, "near-miss 'tubes' -> none (exact names only)");

    group ("parseDeviceSpec: counts and hybrids (signal order preserved)");
    expectSpec ("tube",          { { DeviceType::tube, 1 } },                          "bare type -> count 1");
    expectSpec ("tube:4",        { { DeviceType::tube, 4 } },                          "explicit count");
    expectSpec ("tube:1,bjt:1",  { { DeviceType::tube, 1 }, { DeviceType::bjt, 1 } },  "hybrid (tube first)");
    expectSpec ("pnp:1,tube:1",  { { DeviceType::bjt, 1 }, { DeviceType::tube, 1 } },  "order is the spec's, not sorted");
    expectSpec ("tube:2,tube:1", { { DeviceType::tube, 2 }, { DeviceType::tube, 1 } }, "repeated type stays two entries");
    expectSpec ("diode,dsp:2",   { { DeviceType::diode, 1 }, { DeviceType::dsp, 2 } }, "bare + counted mix");
    expectSpec (" tube:1 , pnp:2 ", { { DeviceType::tube, 1 }, { DeviceType::bjt, 2 } }, "whitespace around tokens tolerated");
    expectSpec ("tube : 2",      { { DeviceType::tube, 2 } },                          "whitespace around the colon tolerated");
    expectSpec ("TUBE:2",        { { DeviceType::tube, 2 } },                          "type is case-folded");

    group ("parseDeviceSpec: malformed input drops entries, never garbage");
    ok (parseDeviceSpec ("").empty(),        "empty spec -> empty");
    ok (parseDeviceSpec (",,").empty(),      "only separators -> empty");
    expectSpec ("tube:1,", { { DeviceType::tube, 1 } }, "trailing comma ignored");
    ok (parseDeviceSpec (":1").empty(),      "missing type dropped");
    ok (parseDeviceSpec ("bogus:2").empty(), "unknown type dropped");
    expectSpec ("tube:1,bogus:2,pnp:1", { { DeviceType::tube, 1 }, { DeviceType::bjt, 1 } },
                "unknown entry dropped, valid neighbours kept");
    ok (parseDeviceSpec ("tube:0").empty(),  "zero count dropped");
    ok (parseDeviceSpec ("tube:-3").empty(), "negative count dropped");
    ok (parseDeviceSpec ("tube:x").empty(),  "non-numeric count dropped");
    ok (parseDeviceSpec ("tube:").empty(),   "colon with empty count dropped (reads as 0)");
    expectSpec ("tube:2.9", { { DeviceType::tube, 2 } }, "count reads the leading integer of '2.9'");

    group ("parseDeviceSpec / deviceSpecCount: the kMaxDeviceGlyphs clamp");
    expectSpec ("tube:99", { { DeviceType::tube, kMaxDeviceGlyphs } }, "per-entry count clamps to 12");
    ok (deviceSpecCount (parseDeviceSpec ("tube:4,pnp:1")) == 5,               "total sums across entries");
    ok (deviceSpecCount (parseDeviceSpec ("tube:99,pnp:99")) == kMaxDeviceGlyphs, "total clamps to 12");
    ok (deviceSpecCount (parseDeviceSpec ("")) == 0,                           "empty spec counts 0");
    ok (deviceSpecCount (DeviceSpec { { DeviceType::tube, 100 } }) == kMaxDeviceGlyphs,
        "hand-built oversize spec still clamps");

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// Theory-first falsification suite for DeviceSpec.h (the preamp device-spec parser).
//
// GRAMMAR, written down independently from the header's doc BEFORE reading the body:
//
//   spec   := ε | entry ("," entry)*          ; comma-separated; empty/whitespace tokens are skipped
//   entry  := WS* type WS* ( ":" WS* count )? WS*
//   type   := a case-insensitive ALIAS of one family:
//               tube  ← tube | valve
//               pnp   ← pnp | npn | bjt | transistor
//               fet   ← fet | jfet | mosfet
//               dsp   ← dsp | chip | ic | digital
//               diode ← diode
//             (anything else ⇒ the family is `none`)
//   count  := the leading integer of the text after ":" (JUCE getIntValue); absent ⇒ 1
//
//   ACCEPT an entry  ⟺  family ≠ none  AND  count > 0.
//   Emitted count is min(count, kMaxDeviceGlyphs=12). Order = input order (never sorted, never merged).
//   deviceSpecCount(spec) = clamp(Σ counts, 0, 12).
//
// Derived INVARIANTS the parser must uphold for ANY input (property-fuzzed below):
//   I1  never crashes / no UB on hostile input (unicode, control bytes, huge counts, deep commas).
//   I2  every emitted entry has family ≠ none and count ∈ [1, 12].
//   I3  #entries ≤ #non-empty comma tokens (a malformed spec yields FEWER entries, never garbage).
//   I4  deviceSpecCount ∈ [0, 12] and equals clamp(Σ emitted counts, 0, 12).
//   I5  emitted families are an in-order subsequence of the tokens' resolved families (order kept,
//       unknown tokens dropped, nothing invented).
//   RT  round-trip: any spec of valid families + counts 1..12 renders to "name:count,…" and parses back
//       to the identical spec.
//
// DeviceSpec.h parses with juce::String, so this registers in the FELITRONICS_APPKIT_TESTS_WITH_JUCE
// tier exactly like DeviceSpecTests.cpp.

#include <felitronics/appkit/DeviceSpec.h>

#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace felitronics::appkit;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

static std::mt19937 rng (0xC0FFEEu);
static int roll (int nExclusive) { return (int) (rng() % (unsigned) nExclusive); }

// A canonical family name (for the round-trip renderer) — never emits `none`.
static const char* canonicalName (DeviceType t)
{
    switch (t)
    {
        case DeviceType::tube:  return "tube";
        case DeviceType::pnp:   return "pnp";
        case DeviceType::fet:   return "fet";
        case DeviceType::dsp:   return "dsp";
        case DeviceType::diode: return "diode";
        case DeviceType::none:  break;
    }
    return "none";
}

// --- independent structural helpers (do NOT reuse the parser's accept logic) -----------------------

// non-empty comma tokens (the count I3 bounds against)
static int nonEmptyTokenCount (const juce::String& s)
{
    int n = 0;
    for (const auto& t : juce::StringArray::fromTokens (s, ",", ""))
        if (t.trim().isNotEmpty())
            ++n;
    return n;
}

// families the tokens resolve to (before the count/accept step) — the I5 superset
static std::vector<DeviceType> tokenFamilies (const juce::String& s)
{
    std::vector<DeviceType> fam;
    for (auto t : juce::StringArray::fromTokens (s, ",", ""))
    {
        t = t.trim();
        if (t.isEmpty())
            continue;
        const auto f = deviceFromString (t.upToFirstOccurrenceOf (":", false, false));
        if (f != DeviceType::none)
            fam.push_back (f);
    }
    return fam;
}

static bool isOrderedSubsequence (const std::vector<DeviceType>& sub, const std::vector<DeviceType>& sup)
{
    size_t j = 0;
    for (const auto d : sub)
    {
        while (j < sup.size() && sup[j] != d) ++j;
        if (j == sup.size()) return false;
        ++j;
    }
    return true;
}

// Assert every documented invariant on one parse result. Returns false on the first violation.
static bool obeysInvariants (const juce::String& s)
{
    const DeviceSpec spec = parseDeviceSpec (s);   // I1: must not crash / UB

    long long sum = 0;
    std::vector<DeviceType> emitted;
    emitted.reserve (spec.size());
    for (const auto& e : spec)
    {
        if (e.first == DeviceType::none)             return false;  // I2
        if (e.second < 1 || e.second > kMaxDeviceGlyphs) return false;  // I2
        sum += e.second;
        emitted.push_back (e.first);
    }

    if ((int) spec.size() > nonEmptyTokenCount (s))  return false;  // I3

    const int total = deviceSpecCount (spec);
    const long long clamped = sum < 0 ? 0 : (sum > kMaxDeviceGlyphs ? kMaxDeviceGlyphs : sum);
    if (total < 0 || total > kMaxDeviceGlyphs)       return false;  // I4
    if ((long long) total != clamped)                return false;  // I4

    if (! isOrderedSubsequence (emitted, tokenFamilies (s))) return false;  // I5

    return true;
}

// --- fuzz string generator: aliases + noise + counts + unicode + separators -----------------------

static juce::String randomFuzzString()
{
    static const std::vector<juce::String> atoms = {
        // valid family aliases
        "tube", "valve", "pnp", "npn", "bjt", "transistor", "fet", "jfet", "mosfet",
        "dsp", "chip", "ic", "digital", "diode",
        // near-misses / unknowns
        "tubes", "bogus", "xyz", "TUBE", "NpN", " valve ",
        // counts and malformed counts
        ":1", ":4", ":12", ":99", ":0", ":-3", ":2.9", ":x", ":", ":999999999999999999999",
        // separators / whitespace / punctuation
        ",", ",,", " , ", " ", "\t", ":", "::", "  ",
        // unicode code points (valid UTF-8, exercises non-ASCII paths)
        juce::String::charToString ((juce::juce_wchar) 0x00E9),   // é
        juce::String::charToString ((juce::juce_wchar) 0x0416),   // Ж
        juce::String::charToString ((juce::juce_wchar) 0x4E2D),   // 中
        juce::String::charToString ((juce::juce_wchar) 0x1F600),  // 😀
    };

    juce::String s;
    const int atomCount = roll (24);   // 0..23 atoms
    for (int i = 0; i < atomCount; ++i)
        s += atoms[(size_t) roll ((int) atoms.size())];
    return s;
}

int main()
{
    std::printf ("felitronics::appkit device-spec theory tests\n");

    // ---------------------------------------------------------------------------------------------
    group ("property fuzz: hostile input never crashes and always obeys the grammar invariants");
    {
        bool allGood = true;
        juce::String firstBad;
        for (int i = 0; i < 20000 && allGood; ++i)
        {
            const juce::String s = randomFuzzString();
            if (! obeysInvariants (s)) { allGood = false; firstBad = s; }
        }
        ok (allGood, allGood ? "20000 random specs obey I1..I5"
                             : "invariant violated on: '" + firstBad.toStdString() + "'");
    }

    // ---------------------------------------------------------------------------------------------
    group ("pathological inputs: still no crash, still invariant-clean");
    {
        std::vector<juce::String> nasty = {
            juce::String(),                                             // empty
            juce::String::repeatedString (",", 2000),                  // deep commas
            juce::String::repeatedString ("tube:12,", 5000),           // thousands of entries
            "tube:" + juce::String::repeatedString ("9", 400),         // 400-digit count
            juce::String::repeatedString ("bogus,", 1000) + "tube",    // long unknown run then valid
            juce::String::repeatedString (juce::String::charToString ((juce::juce_wchar) 0x4E2D), 3000), // unicode wall
            ":::,,:::", "tube:tube:tube", "  \t  ", "\n\r\t",
        };
        bool allGood = true;
        juce::String firstBad;
        for (const auto& s : nasty)
            if (! obeysInvariants (s)) { allGood = false; firstBad = s.substring (0, 40); break; }
        ok (allGood, allGood ? "all pathological inputs obey I1..I5"
                             : "pathological input violated invariants: '" + firstBad.toStdString() + "'");
    }

    // ---------------------------------------------------------------------------------------------
    group ("order preservation + drop + clamp, checked against an independent reference");
    {
        // Each pool entry carries its OWN expected contribution, so the reference spec is built
        // independently of the parser (not by re-running it).
        struct ValidTok  { juce::String text; DeviceType fam; int emitted; };
        const std::vector<ValidTok> valids = {
            { "tube",       DeviceType::tube,  1 },   // bare ⇒ 1
            { "valve:3",    DeviceType::tube,  3 },   // alias folds
            { "NPN:2",      DeviceType::pnp,   2 },   // case folds
            { " bjt : 4 ",  DeviceType::pnp,   4 },   // whitespace around token + colon
            { "mosfet:15",  DeviceType::fet,   kMaxDeviceGlyphs },   // clamps 15 → 12
            { "dsp:7",      DeviceType::dsp,   7 },
            { "diode:1",    DeviceType::diode, 1 },
        };
        const std::vector<juce::String> invalids = {
            "bogus", "bogus:3", "tube:0", "valve:-2", ":5", "fet:x", "tubes:2", "", "   ",
        };

        bool allGood = true;
        for (int iter = 0; iter < 4000 && allGood; ++iter)
        {
            std::vector<juce::String> tokens;
            DeviceSpec expected;
            const int n = 1 + roll (6);
            for (int k = 0; k < n; ++k)
            {
                if (roll (2) == 0)   // valid token
                {
                    const auto& v = valids[(size_t) roll ((int) valids.size())];
                    tokens.push_back (v.text);
                    expected.push_back ({ v.fam, v.emitted });
                }
                else                 // invalid token (contributes nothing)
                {
                    tokens.push_back (invalids[(size_t) roll ((int) invalids.size())]);
                }
            }
            juce::String joined;
            for (size_t k = 0; k < tokens.size(); ++k)
                joined += (k ? "," : "") + tokens[k];

            if (parseDeviceSpec (joined) != expected) { allGood = false;
                ok (false, "mismatch on '" + joined.toStdString() + "'"); }
        }
        ok (allGood, "4000 mixed valid/invalid specs parse to the independent reference (order/drop/clamp)");
    }

    // ---------------------------------------------------------------------------------------------
    group ("round-trip: render a valid spec canonically, parse it back, get the same spec");
    {
        const DeviceType families[] = { DeviceType::tube, DeviceType::pnp, DeviceType::fet,
                                        DeviceType::dsp, DeviceType::diode };
        bool allGood = true;
        juce::String firstBad;
        for (int iter = 0; iter < 4000 && allGood; ++iter)
        {
            DeviceSpec spec;
            const int n = 1 + roll (8);
            for (int k = 0; k < n; ++k)
                spec.push_back ({ families[(size_t) roll (5)], 1 + roll (kMaxDeviceGlyphs) });

            juce::String rendered;
            for (size_t k = 0; k < spec.size(); ++k)
                rendered += juce::String (k ? "," : "")
                          + canonicalName (spec[k].first) + ":" + juce::String (spec[k].second);

            if (parseDeviceSpec (rendered) != spec) { allGood = false; firstBad = rendered; }
        }
        ok (allGood, allGood ? "4000 valid specs survive render → parse unchanged"
                             : "round-trip broke on: '" + firstBad.toStdString() + "'");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

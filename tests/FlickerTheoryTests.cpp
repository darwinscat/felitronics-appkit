// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// Theory-first falsification suite for Flicker.h (the shared "heater glow" shimmer kernel).
//
// The contract is derived from the header's DOCUMENTATION before reading the body, then run:
//   • kRest = 0.84 is the idle level AND the value before the first tick().
//   • tick() advances every channel one frame; level(i) is ALWAYS in [0.5, 1] (documented clamp),
//     out-of-range indices clamp to the end channels.
//   • The modulation (two zero-mean detuned sines + zero-mean jitter) rides on kRest, so the
//     long-run mean sits at the rest+modulation midpoint (≈ kRest) with no monotone drift.
//   • phase is a free-running float advanced by `phase += 1.0f`. THEORY ATTACK (derived, then
//     confirmed by simulation): float32 has a 24-bit significand, so once phase reaches 2^24
//     (= 16 777 216, ~6.5 days into a 30 Hz timer) `phase + 1.0f` rounds back to phase — the
//     accumulator FREEZES and the deterministic shimmer goes permanently static. The kernel now
//     folds phase back below 2^24 every ~1e6 ticks; this suite proves the shimmer keeps breathing
//     when seeded past 2^24 (simulated via the seed constructor — no 16 M-tick loop).
//
// juce_core only (juce::Random / jlimit), so this registers in the FELITRONICS_APPKIT_TESTS_WITH_JUCE
// tier exactly like DeviceSpecTests.cpp.

#include <felitronics/appkit/Flicker.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
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

// Bit-exact float compare that dodges -Wfloat-equal — these checks are deliberately exact (the
// rest value before any tick, index-clamp identity, and the 2^24 rounding fact are all bit-precise).
static bool exactEq (float a, float b) noexcept { return ! (a < b) && ! (b < a); }

// Peak-to-peak of a slice [a, b) of a sample vector.
static float peakToPeak (const std::vector<float>& v, size_t a, size_t b)
{
    float mn = 1.0e9f, mx = -1.0e9f;
    for (size_t i = a; i < b; ++i) { mn = std::min (mn, v[i]); mx = std::max (mx, v[i]); }
    return mx - mn;
}

int main()
{
    std::printf ("felitronics::appkit Flicker theory tests\n");

    // ---------------------------------------------------------------------------------------------
    group ("rest state: before the first tick every channel reads exactly kRest");
    {
        Flicker<8> f;
        bool allRest = true;
        for (int i = 0; i < f.size(); ++i)
            allRest = allRest && exactEq (f.level (i), Flicker<8>::kRest) && exactEq (f[i], Flicker<8>::kRest);
        ok (allRest, "all channels == kRest before any tick");
        ok (Flicker<8>::size() == 8, "size() reports MaxChannels");
    }

    // ---------------------------------------------------------------------------------------------
    group ("bounds invariant: level(i) in [0.5, 1] for ANY tick count and ANY index");
    {
        Flicker<12> f;
        bool inBounds = true;
        for (int t = 0; t < 5000; ++t)
        {
            f.tick();
            for (int i = -3; i < f.size() + 3; ++i)   // includes out-of-range indices
                inBounds = inBounds && (f.level (i) >= 0.5f) && (f.level (i) <= 1.0f);
        }
        ok (inBounds, "5000 ticks × 18 indices all within [0.5, 1]");

        // out-of-range indices clamp to the end channels (documented)
        ok (exactEq (f.level (-100), f.level (0)),            "negative index clamps to channel 0");
        ok (exactEq (f.level (9999), f.level (f.size() - 1)), "huge index clamps to the last channel");
    }

    // ---------------------------------------------------------------------------------------------
    group ("long-run statistics: mean at the rest midpoint (~kRest), no monotone drift");
    {
        Flicker<4> f;
        const int N = 20000;
        double sum = 0.0, sumFirst = 0.0, sumSecond = 0.0;
        for (int t = 0; t < N; ++t)
        {
            f.tick();
            const double l = (double) f.level (0);
            sum += l;
            (t < N / 2 ? sumFirst : sumSecond) += l;
        }
        const double mean  = sum / N;
        const double half1 = sumFirst  / (N / 2);
        const double half2 = sumSecond / (N / 2);
        // Derived: shimmer + jitter are zero-mean, so the mean rides at kRest (0.84); the rare upper
        // clip at 1.0 only nudges it slightly down. Band is generous for RNG variation.
        ok (mean > 0.80 && mean < 0.86, "mean sits near kRest (0.80 .. 0.86)");
        ok (std::abs (half1 - half2) < 0.02, "no monotone drift (first-half vs second-half mean)");
    }

    // ---------------------------------------------------------------------------------------------
    group ("phase-freeze fix: shimmer keeps breathing when seeded past 2^24 ticks");
    {
        // Root cause, as a pure float fact (documents WHY the accumulator freezes):
        ok (exactEq  (16777216.0f + 1.0f, 16777216.0f), "2^24 + 1.0f rounds back to 2^24 (float32 freeze point)");
        ok (! exactEq (8388608.0f + 1.0f, 8388608.0f),  "2^23 + 1.0f still advances (below the freeze point)");

        // Seed the phase AT / PAST the freeze point and run. With the wrap fix the phase folds back
        // into a live range, so the deterministic sine shimmer dominates the last-200-tick window
        // (peak-to-peak ~0.24). WITHOUT the fix the phase would be frozen and only the ±0.025 jitter
        // survives (peak-to-peak ~0.03) — the 0.10 threshold cleanly separates the two.
        for (const float seed : { 16777216.0f /*2^24*/, 33554432.0f /*2^25*/ })
        {
            Flicker<4> f (seed);
            std::vector<float> trace;
            trace.reserve (400);
            bool inBounds = true;
            for (int t = 0; t < 400; ++t)
            {
                f.tick();
                trace.push_back (f.level (0));
                inBounds = inBounds && (f.level (0) >= 0.5f) && (f.level (0) <= 1.0f);
            }
            const float p2p = peakToPeak (trace, 200, 400);
            ok (p2p > 0.10f,
                "seeded phase " + std::to_string ((long long) seed) + ": shimmer alive past 2^24 (p2p " + std::to_string (p2p) + " > 0.10)");
            ok (inBounds, "seeded past 2^24 still respects the [0.5, 1] clamp");
        }
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

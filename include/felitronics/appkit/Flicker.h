// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::Flicker — the family's organic "heater glow" shimmer: a bank of glow levels,
// each advanced one frame per tick() by two detuned sines plus a little random jitter, then one-pole
// smoothed so it breathes rather than strobes. Extracted from the two byte-identical copies in
// OrbitCab (TubeDisplay's power-tube heaters and DeviceStrip's device glyphs) so every glowing
// component in the family flickers the SAME way. juce_core only — drive it from a UI timer (the
// products use 30 Hz) and scale the glow colour's alpha by level(i).
//==============================================================================

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>

namespace felitronics::appkit
{

template <int MaxChannels>
class Flicker
{
    static_assert (MaxChannels > 0, "Flicker needs at least one channel");

public:
    static constexpr float kRest = 0.84f;   // idle glow level — also the value before the first tick()

    Flicker() { levels.fill (kRest); }

    // Seed the free-running animation phase — lets independent banks start out of step so they do not
    // shimmer in lock-step (and lets tests reach the phase-wrap boundary without 16 M real ticks).
    explicit Flicker (float initialPhase) : phase (initialPhase) { levels.fill (kRest); }

    // Advance ALL channels one frame. Verbatim the OrbitCab kernel: a slow sine + a detuned faster
    // sine (each channel phase-offset by 1.7 rad so neighbours never pulse together) + random
    // jitter, clamped to [0.5, 1], then one-pole smoothed (0.7 / 0.3) → organic, never harsh.
    void tick()
    {
        phase += 1.0f;

        // Fold the free-running phase back well below the float32 integer ceiling (2^24 ≈ 16.7 M).
        // Past 2^24 the accumulator `phase += 1.0f` rounds to a no-op, so phase — and with it the
        // deterministic shimmer — would freeze FOREVER (~6.5 days into a 30 Hz timer). Wrapping every
        // kPhaseWrap ticks keeps the increment lossless; the lone phase discontinuity lands once per
        // ~9 h and is imperceptible in a jittered decorative glow. Below kPhaseWrap this branch never
        // fires, so the per-tick output stays byte-for-byte identical to the original kernel.
        if (phase >= kPhaseWrap)
            phase -= kPhaseWrap;

        for (int i = 0; i < MaxChannels; ++i)
        {
            const float ph      = phase * 0.21f + (float) i * 1.7f;
            const float shimmer = 0.10f * std::sin (ph) + 0.05f * std::sin (ph * 2.63f + 1.0f);
            const float jitter  = (rng.nextFloat() - 0.5f) * 0.05f;
            const float target  = juce::jlimit (0.5f, 1.0f, kRest + shimmer + jitter);
            levels[(size_t) i]  = levels[(size_t) i] * 0.7f + target * 0.3f;
        }
    }

    // Channel glow level in [0.5, 1]. Out-of-range indices clamp (callers size the bank generously).
    float level (int i) const noexcept  { return levels[(size_t) juce::jlimit (0, MaxChannels - 1, i)]; }
    float operator[] (int i) const noexcept { return level (i); }

    static constexpr int size() noexcept { return MaxChannels; }

private:
    // Wrap point for the free-running phase (≈ every 9 h at 30 Hz) — kept far below the float32
    // integer ceiling 2^24 so `phase += 1.0f` never rounds to a no-op and stalls the shimmer. See tick().
    static constexpr float kPhaseWrap = 1.0e6f;

    std::array<float, (size_t) MaxChannels> levels {};
    float        phase = 0.0f;
    juce::Random rng;
};

} // namespace felitronics::appkit

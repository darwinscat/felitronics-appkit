// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_core/juce_core.h>

#include <utility>
#include <vector>

//==============================================================================
// felitronics::appkit — the DATA model for a preamp's active device(s): the type enum, the string
// parsers, and the (possibly HYBRID) "device spec". Pure juce_core — NO graphics — so it is
// unit-testable in isolation; the schematic drawing lives in DeviceGlyph.h which includes this.
// Moved verbatim from OrbitCab ui/DeviceSpec.h when the device visuals were promoted here.
//==============================================================================
namespace felitronics::appkit
{

enum class DeviceType { none, tube, pnp, fet, dsp, diode };

inline DeviceType deviceFromString (const juce::String& s)
{
    const auto l = s.trim().toLowerCase();
    if (l == "tube" || l == "valve")          return DeviceType::tube;
    if (l == "pnp"  || l == "npn" || l == "bjt" || l == "transistor") return DeviceType::pnp;
    if (l == "fet"  || l == "jfet" || l == "mosfet") return DeviceType::fet;
    if (l == "dsp"  || l == "chip" || l == "ic" || l == "digital")    return DeviceType::dsp;
    if (l == "diode")                         return DeviceType::diode;
    return DeviceType::none;
}

//==============================================================================
// A device SPEC lists the active devices of a (possibly HYBRID) preamp, in signal order:
//   "tube:1"        → one triode           (Volt-style)
//   "tube:4"        → four triodes          (V4)
//   "pnp:1"         → one transistor        (solid-state ISA)
//   "tube:1,pnp:1"  → a tube AND a transistor (a hybrid — e.g. the ReVolt)
// OrbitCab stores it in the capture metadata under "device". A bare "tube" means count 1; unknown
// types and non-positive counts are dropped, so a malformed spec yields fewer entries, never garbage.
using DeviceSpec = std::vector<std::pair<DeviceType, int>>;

// Glyph-count cap — bounds both the per-entry count and the total drawn row (+ the popup width
// reservation, and DeviceStrip's flicker-bank size).
inline constexpr int kMaxDeviceGlyphs = 12;

inline DeviceSpec parseDeviceSpec (const juce::String& s)
{
    DeviceSpec out;
    for (auto tok : juce::StringArray::fromTokens (s, ",", ""))
    {
        tok = tok.trim();
        if (tok.isEmpty())
            continue;
        const auto type = deviceFromString (tok.upToFirstOccurrenceOf (":", false, false));
        const int  cnt  = tok.contains (":") ? tok.fromFirstOccurrenceOf (":", false, false).trim().getIntValue() : 1;
        if (type != DeviceType::none && cnt > 0)
            out.push_back ({ type, juce::jmin (cnt, kMaxDeviceGlyphs) });
    }
    return out;
}

// Total glyph count across the spec (clamped — bounds the drawn row + the popup width reservation).
inline int deviceSpecCount (const DeviceSpec& spec)
{
    int n = 0;
    for (const auto& p : spec)
    {
        if (p.second <= 0)
            continue;
        n += juce::jmin (p.second, kMaxDeviceGlyphs - n);
        if (n >= kMaxDeviceGlyphs)
            return kMaxDeviceGlyphs;
    }
    return n;
}

} // namespace felitronics::appkit

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

// `ic` and `dsp` are deliberately SEPARATE types. An analogue op-amp and a digital signal path share
// nothing but a DIP package: a Tube Screamer's JRC4558 sits IN the audio path and clips, while a
// Boss GT converts, computes and converts back. One chip glyph for both would hide the single most
// useful thing this row can say about a pedal.
//
// `ic` was an alias for `dsp` before the split. Existing specs that say "ic" now read as ANALOGUE —
// which is what the word almost always meant for the pedals this describes; a caller that really
// means digital has "dsp" and "digital".
enum class DeviceType { none, tube, bjt, fet, dsp, diode, ic };

// The vocabulary is CLOSED: tube · bjt · fet · ic · dsp · diode, one spelling each. It is written by
// the capture tool, not typed by a person, so a forgiving parser buys nothing and costs ambiguity —
// "ic" used to be an accepted spelling of "dsp", which is exactly how an analogue op-amp and a
// digital box ended up drawn as the same part.
//
// "pnp" and "npn" are the one exception: both are bipolar transistors and captures already on disk
// are stamped "pnp". They draw the same symbol. Drop them once those captures are re-stamped "bjt".
inline DeviceType deviceFromString (const juce::String& s)
{
    const auto l = s.trim().toLowerCase();
    if (l == "tube")                              return DeviceType::tube;
    if (l == "bjt" || l == "pnp" || l == "npn")   return DeviceType::bjt;   // legacy capture metadata
    if (l == "fet")                               return DeviceType::fet;
    if (l == "ic")                                return DeviceType::ic;
    if (l == "dsp")                               return DeviceType::dsp;
    if (l == "diode")                             return DeviceType::diode;
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

// A part is drawn ONCE, whatever the device has of it. What differs between families is whether the
// number is worth saying at all — see glyphCountShown below.
inline int glyphsForType (DeviceType t, int count)
{
    if (count <= 0 || t == DeviceType::none)
        return 0;

    return 1;
}

// The count to print beside the glyph, or 0 for none.
//
// For a tube or a diode the number IS the circuit: one tube is single-ended class A and two are
// push-pull; two diodes clip symmetrically and three do not. It has to be said — but said, as "x4",
// not drawn four times. Four small pictures of the same part cost the room that made the part
// legible, and legibility is the entire job of a glyph.
//
// For a transistor or a chip the number says nothing anyone can hear. Presence is the whole message,
// so the count is not shown.
inline int glyphCountShown (DeviceType t, int count)
{
    if (count <= 1)
        return 0;

    switch (t)
    {
        case DeviceType::tube:
        case DeviceType::diode: return count;
        case DeviceType::bjt:
        case DeviceType::fet:
        case DeviceType::ic:
        case DeviceType::dsp:
        case DeviceType::none:
        default:                break;
    }
    return 0;
}

// Total glyph count across the spec (clamped — bounds the drawn row + the popup width reservation).
inline int deviceSpecCount (const DeviceSpec& spec)
{
    int n = 0;
    for (const auto& p : spec)
    {
        const int drawn = glyphsForType (p.first, p.second);
        if (drawn <= 0)
            continue;
        n += juce::jmin (drawn, kMaxDeviceGlyphs - n);
        if (n >= kMaxDeviceGlyphs)
            return kMaxDeviceGlyphs;
    }
    return n;
}

} // namespace felitronics::appkit

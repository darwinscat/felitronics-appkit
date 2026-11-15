// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::InputPick — WHICH INPUTS OF AN AUDIO INTERFACE PLAY, moved from the cab builders (Orbit Bass and
// Guitar Cab Builder 0.5.0) for every standalone of the family: the player picks IN 1..IN n, so an instrument plays
// from whichever input it is plugged into, without repatching (the owner, 15.11.2026: his DI in IN 4, the cabinet's
// mic in IN 3).
//
//   InputChoice   one input, two summed at half each (an amp's L+R), or a pair (a STEREO amp's two takes); its text,
//                 "4", "3+4", "3/4" (IN numbers), is what a store keeps; fittedTo() falls back to IN 1.
//   InputPick     the audio side: every input of the device in, the choice out — one or a sum to every output, a pair
//                 to the first two; every change crossfaded over 20 ms, so a switch never clicks; the peak of every
//                 input kept for its meter, chosen or not. Real-time safe: no allocation, no lock; the choice and the
//                 levels are atomics.
//   InputMemory   the choice kept per device by its name — a workplace setting, never in a preset — read and written
//                 through the app's own store (two functions: a JUCE PropertiesFile, say), as text. No file of its own.
//
// Threads: choose()/chosen()/takeLevel() on the message thread; prepare() before the device starts (no process() at
// the same time); process() on the audio thread. JUCE-free, header-only.
//==============================================================================

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace felitronics::appkit
{

/** What plays. Inputs are counted from 0 (IN 1 is 0); `b` matters for a sum and a pair only. */
struct InputChoice
{
    enum class Kind : std::uint8_t { one, sum, pair };

    static constexpr int maxInputs = 64;

    Kind kind = Kind::one;
    int a = 0, b = 0;

    static constexpr InputChoice single (int in) noexcept { return { Kind::one, in, 0 }; }
    static constexpr InputChoice summed (int first, int second) noexcept { return { Kind::sum, first, second }; }
    static constexpr InputChoice paired (int left, int right) noexcept { return { Kind::pair, left, right }; }

    friend constexpr bool operator== (const InputChoice& x, const InputChoice& y) noexcept
    {
        return x.kind == y.kind && x.a == y.a && (x.kind == Kind::one || x.b == y.b);
    }
    friend constexpr bool operator!= (const InputChoice& x, const InputChoice& y) noexcept { return ! (x == y); }

    /** Every input it names is one of the device's `numInputs`. */
    constexpr bool fits (int numInputs) const noexcept
    {
        return has (a, numInputs) && (kind == Kind::one || has (b, numInputs));
    }

    static constexpr bool has (int in, int numInputs) noexcept { return in >= 0 && in < numInputs && in < maxInputs; }

    /** Itself where the device has its inputs; else IN 1 — a sum or a pair of IN 1 and IN 2 where there are two, so a
        STEREO amp keeps its pair. */
    constexpr InputChoice fittedTo (int numInputs) const noexcept
    {
        if (fits (numInputs))
            return *this;
        if (kind != Kind::one && numInputs >= 2)
            return { kind, 0, 1 };
        return single (0);
    }

    /** "4", "3+4" or "3/4": the IN numbers, from 1. */
    std::string toText() const
    {
        const auto in = [] (int i) { return std::to_string (i + 1); };
        switch (kind)
        {
            case Kind::sum:  return in (a) + "+" + in (b);
            case Kind::pair: return in (a) + "/" + in (b);
            case Kind::one:  break;
        }
        return in (a);
    }

    /** toText's form read back; nothing for anything else (a number past maxInputs, a nought, a stray character). */
    static std::optional<InputChoice> fromText (const std::string& text)
    {
        const auto number = [] (const std::string& s) -> std::optional<int>
        {
            if (s.empty() || s.size() > 2 || ! std::all_of (s.begin(), s.end(), [] (char c) { return c >= '0' && c <= '9'; }))
                return std::nullopt;
            const int n = std::stoi (s);
            if (n < 1 || n > maxInputs)
                return std::nullopt;
            return n - 1;
        };
        const auto at = text.find_first_of ("+/");
        if (at == std::string::npos)
        {
            if (const auto n = number (text))
                return single (*n);
            return std::nullopt;
        }
        const auto first = number (text.substr (0, at)), second = number (text.substr (at + 1));
        if (! first || ! second)
            return std::nullopt;
        return InputChoice { text[at] == '+' ? Kind::sum : Kind::pair, *first, *second };
    }
};

/** The audio side: the device's inputs in, the choice out. See the file's head. */
class InputPick
{
public:
    static constexpr int maxInputs = InputChoice::maxInputs;

    InputPick() noexcept
    {
        for (auto& l : levels)
            l.store (0.0f, std::memory_order_relaxed);
        prepare (48000.0, ~std::uint64_t (0));
    }

    /** What plays from now on, crossfaded in over 20 ms. Any thread. */
    void choose (InputChoice c) noexcept { wanted.store (pack (c), std::memory_order_release); }
    InputChoice chosen() const noexcept { return unpack (wanted.load (std::memory_order_acquire)); }

    /** Before the device starts: its rate, and which of its inputs it opened — input k's bit set — since a device hands
        process() the open ones alone, in order. The choice is taken as it stands, no fade. */
    void prepare (double sampleRate, std::uint64_t openInputs) noexcept
    {
        for (int ch = 0, slot = 0; ch < maxInputs; ++ch)
            slotOf[(std::size_t) ch] = ((openInputs >> ch) & 1u) != 0 ? slot++ : -1;
        step = (float) (1.0 / (std::max (1.0, sampleRate) * fadeSeconds));
        heard = chosen();
        fading = false;
    }

    /** Audio thread. `in` are the inputs the device hands over (numIn of them, as prepare() said), `out` the app's
        channels, which must not share memory with `in`: one input or a sum written to every one, a pair to the first
        two (the rest silent; with one output, the pair's first alone). Every input's peak kept for takeLevel(). */
    void process (const float* const* in, int numIn, float* const* out, int numOut, int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;
        measure (in, numIn, numSamples);

        const auto want = chosen();
        if (want != heard && ! fading)
        {
            leaving = heard;
            heard = want;
            fading = true;
            fade = 0.0f;
        }
        // The way from what was to what is, the same for every output: `from` at the block's start, a step a sample.
        const float from = fade;
        for (int o = 0; o < numOut; ++o)
        {
            float* dst = out[o];
            if (dst == nullptr)
                continue;
            const Source now = sourceOf (heard, o, in, numIn);
            if (! fading)
            {
                for (int i = 0; i < numSamples; ++i)
                    dst[i] = now.at (i);
                continue;
            }
            const Source was = sourceOf (leaving, o, in, numIn);
            for (int i = 0; i < numSamples; ++i)
            {
                const float f = std::min (1.0f, from + step * (float) (i + 1));
                dst[i] = f * now.at (i) + (1.0f - f) * was.at (i);
            }
        }
        if (fading)
        {
            fade = std::min (1.0f, from + step * (float) numSamples);
            fading = fade < 1.0f;
        }
    }

    /** The loudest sample input `input` has had since the last ask (1 is full scale); nought for one not open. */
    float takeLevel (int input) noexcept
    {
        return input >= 0 && input < maxInputs ? levels[(std::size_t) input].exchange (0.0f, std::memory_order_relaxed) : 0.0f;
    }

private:
    static constexpr double fadeSeconds = 0.02;

    /** One output's signal from the inputs: up to two of them, each at its weight. */
    struct Source
    {
        const float* x = nullptr;
        const float* y = nullptr;
        float wx = 0.0f, wy = 0.0f;

        float at (int i) const noexcept
        {
            return (x != nullptr ? wx * x[i] : 0.0f) + (y != nullptr ? wy * y[i] : 0.0f);
        }
    };

    std::array<int, maxInputs> slotOf {};
    std::atomic<std::uint32_t> wanted { 0 };
    std::array<std::atomic<float>, maxInputs> levels;
    InputChoice heard, leaving;   // audio thread: what plays, and what it is crossfading from
    bool fading = false;
    float fade = 1.0f, step = 0.0f;

    static std::uint32_t pack (InputChoice c) noexcept
    {
        const auto in = [] (int i) { return (std::uint32_t) std::clamp (i, 0, maxInputs - 1); };
        return (std::uint32_t) c.kind << 16 | in (c.a) << 8 | in (c.b);
    }

    static InputChoice unpack (std::uint32_t v) noexcept
    {
        const auto kind = (v >> 16) & 0xffu;
        return { kind == 1u ? InputChoice::Kind::sum : kind == 2u ? InputChoice::Kind::pair : InputChoice::Kind::one,
                 (int) ((v >> 8) & 0xffu), (int) (v & 0xffu) };
    }

    const float* channel (int input, const float* const* in, int numIn) const noexcept
    {
        const int slot = input >= 0 && input < maxInputs ? slotOf[(std::size_t) input] : -1;
        return slot >= 0 && slot < numIn ? in[slot] : nullptr;
    }

    Source sourceOf (const InputChoice& c, int output, const float* const* in, int numIn) const noexcept
    {
        switch (c.kind)
        {
            case InputChoice::Kind::sum:
                return { channel (c.a, in, numIn), channel (c.b, in, numIn), 0.5f, 0.5f };
            case InputChoice::Kind::pair:
                if (output > 1)
                    return {};
                return { channel (output == 0 ? c.a : c.b, in, numIn), nullptr, 1.0f, 0.0f };
            case InputChoice::Kind::one:
                break;
        }
        return { channel (c.a, in, numIn), nullptr, 1.0f, 0.0f };
    }

    void measure (const float* const* in, int numIn, int numSamples) noexcept
    {
        for (int ch = 0; ch < maxInputs; ++ch)
            if (const float* x = channel (ch, in, numIn))
            {
                float peak = 0.0f;
                for (int i = 0; i < numSamples; ++i)
                    peak = std::max (peak, std::abs (x[i]));
                auto& l = levels[(std::size_t) ch];
                if (peak > l.load (std::memory_order_relaxed))
                    l.store (peak, std::memory_order_relaxed);
            }
    }
};

/** THE CHOICE A DEVICE PLAYS, kept by the device's name in the app's own store — a workplace setting, never in a preset,
    a recording or a link. Key "inputOf <device name>", value InputChoice's text: a bare number (the cab builders'
    0.5.0 kept those) reads as one input. */
struct InputMemory
{
    std::function<std::string (const std::string& key)> read;                       // "" for a key never written
    std::function<void (const std::string& key, const std::string& value)> write;

    static std::string keyOf (const std::string& device) { return "inputOf " + device; }

    /** A choice kept for `device`, whatever inputs it has now. */
    bool knows (const std::string& device) const { return read && InputChoice::fromText (read (keyOf (device))).has_value(); }

    /** The choice kept for `device`, fitted to the inputs it has now (InputChoice::fittedTo: IN 1 for one gone), else
        `otherwise`, fitted too. What is kept stays as it was, for the device's return. */
    InputChoice recall (const std::string& device, int numInputs, InputChoice otherwise = {}) const
    {
        const auto kept = read ? InputChoice::fromText (read (keyOf (device))) : std::nullopt;
        return kept.value_or (otherwise).fittedTo (numInputs);
    }

    void remember (const std::string& device, InputChoice c) const
    {
        if (write)
            write (keyOf (device), c.toText());
    }
};

} // namespace felitronics::appkit

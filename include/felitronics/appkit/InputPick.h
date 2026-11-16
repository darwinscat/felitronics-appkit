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
//                 to the first two; every change crossfaded within 20 ms, from wherever the sound stands, so a switch
//                 never clicks; the peak of every input kept for its meter, chosen or not. Real-time safe: no
//                 allocation, no lock; the choice and the levels are atomics.
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
    constexpr InputChoice fittedTo (int numInputs) const noexcept { return fittedTo (firstInputs (numInputs)); }

    /** The same for a device that opened only some of its inputs (input k's bit set): itself where they are open; else
        the first input open — the first two for a sum or a pair — so a fallback is never a closed input that plays
        silence; IN 1 where none is open. */
    constexpr InputChoice fittedTo (std::uint64_t openInputs) const noexcept
    {
        const auto open = [openInputs] (int in) { return in >= 0 && in < maxInputs && ((openInputs >> in) & 1u) != 0; };
        if (open (a) && (kind == Kind::one || open (b)))
            return *this;
        int first = -1, second = -1;
        for (int in = 0; in < maxInputs && second < 0; ++in)
            if (open (in))
                (first < 0 ? first : second) = in;
        if (first < 0)
            return single (0);
        if (kind != Kind::one && second >= 0)
            return { kind, first, second };
        return single (first);
    }

    /** The bits of a device's first `numInputs` inputs. */
    static constexpr std::uint64_t firstInputs (int numInputs) noexcept
    {
        return numInputs <= 0 ? 0 : numInputs >= maxInputs ? ~std::uint64_t (0) : (std::uint64_t (1) << numInputs) - 1;
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

/** The audio side: the device's inputs in, the choice out. See the file's head.

    Every input reaches an output through a gain of its own, and every gain moves toward what the choice asks of it by
    full scale in 20 ms at most: a change sounds as a crossfade wherever the sound stands, also halfway through the last
    one, and is through within 20 ms. Three lanes: the first output, the second, and the rest (one input or a sum on all
    three, a pair's two sides on the first two and silence on the rest). */
class InputPick
{
public:
    static constexpr int maxInputs = InputChoice::maxInputs;

    InputPick() noexcept { prepare (48000.0, ~std::uint64_t (0)); }

    /** What plays from now on, crossfaded in within 20 ms. Any thread. */
    void choose (InputChoice c) noexcept { wanted.store (pack (c), std::memory_order_release); }
    InputChoice chosen() const noexcept { return unpack (wanted.load (std::memory_order_acquire)); }

    /** Before the device starts: its rate, and which of its inputs it opened — input k's bit set — since a device hands
        process() the open ones alone, in order. The choice is taken as it stands, no fade; the meters start empty. */
    void prepare (double sampleRate, std::uint64_t openInputs) noexcept
    {
        for (int ch = 0, slot = 0; ch < maxInputs; ++ch)
            slotOf[(std::size_t) ch] = ((openInputs >> ch) & 1u) != 0 ? slot++ : -1;
        for (auto& l : levels)
            l.store (0.0f, std::memory_order_relaxed);
        step = (float) (1.0 / (std::max (1.0, sampleRate) * fadeSeconds));
        aim (chosen());
        gain = target;
        moving = 0;
    }

    /** Audio thread. `in` are the inputs the device hands over (numIn of them, as prepare() said), `out` the app's
        channels, which must not share memory with `in`: one input or a sum written to every one, a pair to the first
        two (the rest silent; with one output, the pair's first alone). Every input's peak kept for takeLevel(). */
    void process (const float* const* in, int numIn, float* const* out, int numOut, int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;
        measure (in, numIn, numSamples);

        if (const auto want = chosen(); want != aimedAt)
            aim (want);

        // The rest of the outputs share the third lane: the first of them is written, the others copy it.
        float* rest = nullptr;
        for (int o = 2; o < numOut && rest == nullptr; ++o)
            rest = out[o];
        float* const lanes[] { numOut > 0 ? out[0] : nullptr, numOut > 1 ? out[1] : nullptr, rest };
        for (int lane = 0; lane < numLanes; ++lane)
            mix (lane, lanes[lane], in, numIn, numSamples);
        for (int o = 2; o < numOut; ++o)
            if (out[o] != nullptr && out[o] != rest && rest != nullptr)
                std::copy (rest, rest + numSamples, out[o]);
        moving = std::max (0, moving - numSamples);
        if (moving == 0)
            gain = target;   // every gain where it was going, exactly
    }

    /** The loudest sample input `input` has had since the last ask (1 is full scale); nought for one not open. */
    float takeLevel (int input) noexcept
    {
        return input >= 0 && input < maxInputs ? levels[(std::size_t) input].exchange (0.0f, std::memory_order_relaxed) : 0.0f;
    }

private:
    static constexpr double fadeSeconds = 0.02;
    static constexpr int numLanes = 3;
    using Gains = std::array<std::array<float, maxInputs>, numLanes>;

    std::array<int, maxInputs> slotOf {};
    std::atomic<std::uint32_t> wanted { 0 };
    std::array<std::atomic<float>, maxInputs> levels {};
    // Audio thread: each input's gain on each lane as it plays, where the choice wants it, the choice those are for,
    // and how many samples until every gain is there.
    Gains gain {}, target {};
    InputChoice aimedAt;
    int moving = 0;
    float step = 0.0f;

    void aim (InputChoice c) noexcept
    {
        aimedAt = c;
        for (auto& lane : target)
            lane.fill (0.0f);
        const auto set = [this] (int lane, int input, float g)
        {
            if (input >= 0 && input < maxInputs)
                target[(std::size_t) lane][(std::size_t) input] += g;
        };
        switch (c.kind)
        {
            case InputChoice::Kind::sum:
                for (int lane = 0; lane < numLanes; ++lane)
                {
                    set (lane, c.a, 0.5f);
                    set (lane, c.b, 0.5f);
                }
                break;
            case InputChoice::Kind::pair:
                set (0, c.a, 1.0f);
                set (1, c.b, 1.0f);
                break;
            case InputChoice::Kind::one:
                for (int lane = 0; lane < numLanes; ++lane)
                    set (lane, c.a, 1.0f);
                break;
        }
        moving = (int) std::ceil (1.0f / step) + 2;   // the longest way, nought to full scale, and samples to spare
    }

    static float toward (float now, float to, float by) noexcept { return now < to ? std::min (to, now + by) : std::max (to, now - by); }

    /** One lane: its inputs, each at its gain, into `dst` (none: the gains still move, as if heard). */
    void mix (int lane, float* dst, const float* const* in, int numIn, int numSamples) noexcept
    {
        auto& g = gain[(std::size_t) lane];
        const auto& t = target[(std::size_t) lane];
        std::array<int, maxInputs> live {};   // the inputs this lane hears or is leaving
        int n = 0;
        for (int ch = 0; ch < maxInputs; ++ch)
            if (g[(std::size_t) ch] > 0.0f || t[(std::size_t) ch] > 0.0f)
                live[(std::size_t) n++] = ch;

        if (dst == nullptr)
        {
            for (int k = 0; k < n; ++k)
            {
                const auto ch = (std::size_t) live[(std::size_t) k];
                g[ch] = toward (g[ch], t[ch], step * (float) numSamples);
            }
            return;
        }
        std::fill (dst, dst + numSamples, 0.0f);
        for (int k = 0; k < n; ++k)
        {
            const auto ch = (std::size_t) live[(std::size_t) k];
            const float* x = channel ((int) ch, in, numIn);
            float now = g[ch];
            const float to = t[ch];
            for (int i = 0; i < numSamples; ++i)
            {
                now = toward (now, to, step);
                if (x != nullptr)
                    dst[i] += now * x[i];
            }
            g[ch] = now;
        }
    }

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

    void measure (const float* const* in, int numIn, int numSamples) noexcept
    {
        for (int ch = 0; ch < maxInputs; ++ch)
            if (const float* x = channel (ch, in, numIn))
            {
                float peak = 0.0f;
                for (int i = 0; i < numSamples; ++i)
                    peak = std::max (peak, std::abs (x[i]));
                // Raised, never lowered, also while the meters take it away: no peak lost between a look and a write.
                auto& l = levels[(std::size_t) ch];
                float was = l.load (std::memory_order_relaxed);
                while (peak > was && ! l.compare_exchange_weak (was, peak, std::memory_order_relaxed))
                {
                }
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
        return recall (device, InputChoice::firstInputs (numInputs), otherwise);
    }

    /** ...for a device that opened only some of its inputs: fitted to those open. */
    InputChoice recall (const std::string& device, std::uint64_t openInputs, InputChoice otherwise = {}) const
    {
        const auto kept = read ? InputChoice::fromText (read (keyOf (device))) : std::nullopt;
        return kept.value_or (otherwise).fittedTo (openInputs);
    }

    void remember (const std::string& device, InputChoice c) const
    {
        if (write)
            write (keyOf (device), c.toText());
    }
};

} // namespace felitronics::appkit

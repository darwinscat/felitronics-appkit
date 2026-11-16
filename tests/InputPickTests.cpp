// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// InputPick.h, JUCE-free: the input chosen is what plays, also when the device hands over only some of its inputs; a
// change is crossfaded over 20 ms with no step, and one made halfway through a crossfade is through 20 ms after it; a
// sum is the two at half each, a pair goes one to each output, the outputs past the second follow; every open input is
// measured, a closed one reads nought, and a new device starts the meters empty; the text a store keeps reads back, junk does not; a choice whose
// inputs are gone falls back to IN 1; the memory keeps a device's choice by its name and reads the cab builders'
// bare numbers as one input.

#include <felitronics/appkit/InputPick.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using felitronics::appkit::InputChoice;
using felitronics::appkit::InputMemory;
using felitronics::appkit::InputPick;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

static bool near (float x, float y) { return std::abs (x - y) < 1.0e-5f; }

namespace
{
constexpr double rate = 48000.0;
constexpr int block = 256;
constexpr int fadeSamples = 960;   // 20 ms at 48 kHz

/** Blocks through a pick: each input the device hands over a level of its own, two outputs kept, every sample. */
struct Rig
{
    InputPick pick;
    std::vector<float> levelOf;                    // by the inputs handed over, in order
    std::vector<float> left, right;                // every sample run

    Rig (std::uint64_t open, std::vector<float> levels) : levelOf (std::move (levels)) { pick.prepare (rate, open); }

    std::vector<float> third;                      // the third output, when run with three

    void run (int samples, int outputs = 2)
    {
        std::vector<std::vector<float>> in (levelOf.size(), std::vector<float> ((std::size_t) block));
        std::vector<const float*> ins;
        for (std::size_t k = 0; k < levelOf.size(); ++k)
        {
            std::fill (in[k].begin(), in[k].end(), levelOf[k]);
            ins.push_back (in[k].data());
        }
        std::vector<float> l ((std::size_t) block), r ((std::size_t) block), t ((std::size_t) block);
        float* outs[] { l.data(), r.data(), t.data() };
        for (int done = 0; done < samples; done += block)
        {
            pick.process (ins.data(), (int) ins.size(), outs, outputs, block);
            left.insert (left.end(), l.begin(), l.end());
            right.insert (right.end(), r.begin(), r.end());
            third.insert (third.end(), t.begin(), t.end());
        }
    }

    float biggestStep (const std::vector<float>& x, std::size_t from) const
    {
        float most = 0.0f;
        for (std::size_t i = from + 1; i < x.size(); ++i)
            most = std::max (most, std::abs (x[i] - x[i - 1]));
        return most;
    }
};

std::uint64_t firstN (int n) { return (std::uint64_t (1) << n) - 1; }
}

int main()
{
    std::printf ("felitronics::appkit input-pick tests\n");

    group ("one input: the one chosen plays, to every output");
    {
        Rig rig (firstN (4), { 0.1f, 0.2f, 0.3f, 0.4f });
        rig.run (block);
        ok (near (rig.left.back(), 0.1f) && near (rig.right.back(), 0.1f), "IN 1 plays at first, on both outputs");
        rig.pick.choose (InputChoice::single (3));
        const auto from = rig.left.size();
        rig.run (fadeSamples + 2 * block);
        ok (near (rig.left.back(), 0.4f) && near (rig.right.back(), 0.4f), "IN 4 plays once chosen");
        ok (rig.biggestStep (rig.left, from - 1) <= 0.3f / (float) fadeSamples * 1.01f, "the change has no step");
        int between = 0;
        for (std::size_t i = from; i < rig.left.size(); ++i)
            between += rig.left[i] > 0.1001f && rig.left[i] < 0.3999f;
        ok (std::abs (between - fadeSamples) <= 2, "the crossfade takes 20 ms");
        ok (rig.pick.chosen() == InputChoice::single (3), "chosen() says what was chosen");

        // IN 2 chosen, then IN 1 five milliseconds into that crossfade: IN 1 within 20 ms of its choice, no step on the way.
        rig.pick.choose (InputChoice::single (1));
        const auto turn = rig.left.size();
        rig.run (block);                               // 5.3 ms of the way to IN 2
        rig.pick.choose (InputChoice::single (0));
        const auto again = rig.left.size();
        rig.run (fadeSamples + block);
        ok (near (rig.left[again + (std::size_t) fadeSamples + 1], 0.1f), "a choice made halfway through a crossfade is through 20 ms later");
        ok (rig.biggestStep (rig.left, turn - 1) <= (0.4f + 0.2f + 0.1f) / (float) fadeSamples * 1.01f, "...with no step on the way");
    }

    group ("a sum: the two at half each; a pair: one to each output");
    {
        Rig rig (firstN (4), { 0.1f, 0.2f, 0.3f, 0.4f });
        rig.pick.choose (InputChoice::summed (2, 3));
        rig.pick.prepare (rate, firstN (4));        // taken as it stands, no fade
        rig.run (block);
        ok (near (rig.left.front(), 0.35f) && near (rig.right.front(), 0.35f), "IN 3 + IN 4 is their half each, on both outputs");
        rig.pick.choose (InputChoice::paired (2, 3));
        const auto from = rig.left.size();
        rig.run (fadeSamples + 2 * block);
        ok (near (rig.left.back(), 0.3f) && near (rig.right.back(), 0.4f), "IN 3 / IN 4: IN 3 to the first output, IN 4 to the second");
        ok (rig.biggestStep (rig.left, from - 1) <= (0.3f + 0.4f) / (float) fadeSamples * 1.01f
            && rig.biggestStep (rig.right, from - 1) <= (0.3f + 0.4f) / (float) fadeSamples * 1.01f, "sum to pair has no step on either output");
        rig.pick.choose (InputChoice::single (0));
        rig.run (fadeSamples + 2 * block, 1);
        ok (near (rig.left.back(), 0.1f), "one output: the choice there alone");

        // Three outputs: the third follows one input, falls silent, without a step, for a pair.
        rig.run (block, 3);
        ok (near (rig.third.back(), 0.1f), "the outputs past the second carry one input");
        rig.pick.choose (InputChoice::paired (2, 3));
        const auto split = rig.third.size();
        rig.run (fadeSamples + 2 * block, 3);
        ok (near (rig.third.back(), 0.0f) && near (rig.left.back(), 0.3f) && near (rig.right.back(), 0.4f),
            "...and fall silent for a pair");
        ok (rig.biggestStep (rig.third, split - 1) <= 0.1f / (float) fadeSamples * 1.01f, "...without a step");
    }

    group ("a device that opened only some of its inputs");
    {
        Rig rig ((std::uint64_t (1) << 1) | (std::uint64_t (1) << 3), { 0.2f, 0.4f });   // IN 2 and IN 4, handed over in order
        rig.pick.choose (InputChoice::single (3));
        rig.pick.prepare (rate, (std::uint64_t (1) << 1) | (std::uint64_t (1) << 3));
        rig.run (block);
        ok (near (rig.left.back(), 0.4f), "IN 4 plays when it is the second handed over");
        ok (near (rig.pick.takeLevel (1), 0.2f) && near (rig.pick.takeLevel (3), 0.4f), "the open inputs measured by their own number");
        rig.pick.choose (InputChoice::single (0));
        rig.run (fadeSamples + 2 * block);
        ok (near (rig.left.back(), 0.0f), "an input not open plays silence");
    }

    group ("no outputs, or a missing one");
    {
        Rig rig (firstN (4), { 0.1f, 0.2f, 0.3f, 0.4f });
        rig.pick.choose (InputChoice::single (3));
        rig.run (fadeSamples + 2 * block, 0);          // nothing written, the crossfade still runs its course
        rig.run (block, 2);
        ok (near (rig.left.back(), 0.4f) && near (rig.left[rig.left.size() - (std::size_t) block], 0.4f),
            "with no outputs a change goes on as if heard, and is through");
        ok (near (rig.pick.takeLevel (2), 0.3f), "...the meters measured all the same");

        std::vector<float> in0 ((std::size_t) block, 0.1f), in1 ((std::size_t) block, 0.2f), right ((std::size_t) block, -1.0f);
        const float* ins[] { in0.data(), in1.data() };
        float* outs[] { nullptr, right.data() };
        InputPick pick;
        pick.prepare (rate, firstN (2));
        pick.choose (InputChoice::paired (0, 1));
        pick.prepare (rate, firstN (2));
        pick.process (ins, 2, outs, 2, block);
        ok (near (right.back(), 0.2f), "a missing first output: the second still written");
    }

    group ("the meters");
    {
        Rig rig (firstN (4), { 0.1f, -0.5f, 0.3f, 0.0f });
        rig.run (block);
        ok (near (rig.pick.takeLevel (1), 0.5f), "a negative level measured as its size");
        ok (near (rig.pick.takeLevel (2), 0.3f), "an input not chosen is measured too");
        ok (near (rig.pick.takeLevel (2), 0.0f), "a level read is cleared until the next block");
        ok (near (rig.pick.takeLevel (9), 0.0f) && near (rig.pick.takeLevel (-1), 0.0f) && near (rig.pick.takeLevel (64), 0.0f),
            "an input not open, or none at all, reads nought");
        rig.run (block);
        rig.pick.prepare (rate, firstN (1));           // another device, IN 1 alone, before the meters were read
        ok (near (rig.pick.takeLevel (1), 0.0f), "a new device starts the meters empty");
    }

    group ("the text a store keeps");
    {
        ok (InputChoice::single (3).toText() == "4" && InputChoice::summed (2, 3).toText() == "3+4"
            && InputChoice::paired (0, 1).toText() == "1/2", "toText: IN numbers, + for a sum, / for a pair");
        for (const auto c : { InputChoice::single (0), InputChoice::single (63), InputChoice::summed (2, 3), InputChoice::paired (5, 4) })
            ok (InputChoice::fromText (c.toText()) == c, "reads back: " + c.toText());
        for (const char* junk : { "", "0", "65", "100", "3+", "+4", "3+4+5", "3-4", "a", " 4", "4 ", "-1", "3/0" })
            ok (! InputChoice::fromText (junk).has_value(), std::string ("refused: \"") + junk + "\"");
    }

    group ("a choice whose inputs are gone falls back to IN 1");
    {
        ok (InputChoice::single (3).fittedTo (8) == InputChoice::single (3), "one the device has stays");
        ok (InputChoice::single (3).fittedTo (2) == InputChoice::single (0), "one it has not: IN 1");
        ok (InputChoice::summed (2, 5).fittedTo (4) == InputChoice::summed (0, 1), "a sum it has not: IN 1 + IN 2");
        ok (InputChoice::paired (2, 3).fittedTo (2) == InputChoice::paired (0, 1), "a pair it has not: IN 1 / IN 2");
        ok (InputChoice::paired (0, 1).fittedTo (1) == InputChoice::single (0), "a pair on a one-input device: IN 1");
        ok (InputChoice::single (0).fittedTo (0) == InputChoice::single (0), "no inputs at all: IN 1");

        const std::uint64_t threeFour = (std::uint64_t (1) << 2) | (std::uint64_t (1) << 3);   // IN 3 and IN 4 open alone
        ok (InputChoice::single (3).fittedTo (threeFour) == InputChoice::single (3), "an open input stays");
        ok (InputChoice::single (0).fittedTo (threeFour) == InputChoice::single (2), "a closed one: the first input open, not a silent IN 1");
        ok (InputChoice::summed (0, 1).fittedTo (threeFour) == InputChoice::summed (2, 3), "a closed sum: the first two open");
        ok (InputChoice::paired (0, 3).fittedTo (std::uint64_t (1) << 3) == InputChoice::single (3), "a pair with one input open: that one");
        ok (InputChoice::single (5).fittedTo (std::uint64_t (0)) == InputChoice::single (0), "none open: IN 1");
        ok (InputChoice::firstInputs (64) == ~std::uint64_t (0) && InputChoice::firstInputs (3) == 7u && InputChoice::firstInputs (-1) == 0u,
            "firstInputs: the first n bits");
    }

    group ("the memory, by the device's name");
    {
        std::map<std::string, std::string> store;
        const InputMemory memory { [&store] (const std::string& k) { const auto at = store.find (k); return at != store.end() ? at->second : std::string(); },
                                   [&store] (const std::string& k, const std::string& v) { store[k] = v; } };
        ok (! memory.knows ("Scarlett 18i20") && memory.recall ("Scarlett 18i20", 18) == InputChoice::single (0), "a device not seen: IN 1");
        ok (memory.recall ("Scarlett 18i20", 18, InputChoice::single (2)) == InputChoice::single (2), "...or what the app offers");
        memory.remember ("Scarlett 18i20", InputChoice::summed (2, 3));
        ok (store["inputOf Scarlett 18i20"] == "3+4", "kept as text under inputOf <name>");
        ok (memory.knows ("Scarlett 18i20") && memory.recall ("Scarlett 18i20", 18) == InputChoice::summed (2, 3), "recalled for the device");
        ok (memory.recall ("Scarlett 18i20", 2) == InputChoice::summed (0, 1) && store["inputOf Scarlett 18i20"] == "3+4",
            "fitted to fewer inputs, the kept choice left for the device's return");
        store["inputOf UMC404HD"] = "4";   // the cab builders' 0.5.0
        ok (memory.recall ("UMC404HD", 4) == InputChoice::single (3), "a bare number reads as one input");
        store["inputOf Broken"] = "x";
        ok (! memory.knows ("Broken") && memory.recall ("Broken", 4) == InputChoice::single (0), "junk kept reads as not kept");
        ok (memory.recall ("Never seen", (std::uint64_t (1) << 2) | (std::uint64_t (1) << 3)) == InputChoice::single (2),
            "a device not seen, IN 1 closed: the first input open");
        const InputMemory none {};
        ok (none.recall ("Any", 4) == InputChoice::single (0) && ! none.knows ("Any"), "no store: IN 1, nothing kept");
        none.remember ("Any", InputChoice::single (1));
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

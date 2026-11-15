// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// InputPick.h, JUCE-free: the input chosen is what plays, also when the device hands over only some of its inputs; a
// change is crossfaded over 20 ms with no step; a sum is the two at half each, a pair goes one to each output; every
// open input is measured, a closed one reads nought; the text a store keeps reads back, junk does not; a choice whose
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

    void run (int samples, int outputs = 2)
    {
        std::vector<std::vector<float>> in (levelOf.size(), std::vector<float> ((std::size_t) block));
        std::vector<const float*> ins;
        for (std::size_t k = 0; k < levelOf.size(); ++k)
        {
            std::fill (in[k].begin(), in[k].end(), levelOf[k]);
            ins.push_back (in[k].data());
        }
        std::vector<float> l ((std::size_t) block), r ((std::size_t) block);
        float* outs[] { l.data(), r.data() };
        for (int done = 0; done < samples; done += block)
        {
            pick.process (ins.data(), (int) ins.size(), outs, outputs, block);
            left.insert (left.end(), l.begin(), l.end());
            right.insert (right.end(), r.begin(), r.end());
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
        ok (rig.biggestStep (rig.left, from - 1) <= 0.05f / (float) fadeSamples * 1.01f
            && rig.biggestStep (rig.right, from - 1) <= 0.05f / (float) fadeSamples * 1.01f, "sum to pair has no step on either output");
        rig.pick.choose (InputChoice::single (0));
        rig.run (fadeSamples + 2 * block, 1);
        ok (near (rig.left.back(), 0.1f), "one output: the choice there alone");
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

    group ("the meters");
    {
        Rig rig (firstN (4), { 0.1f, -0.5f, 0.3f, 0.0f });
        rig.run (block);
        ok (near (rig.pick.takeLevel (1), 0.5f), "a negative level measured as its size");
        ok (near (rig.pick.takeLevel (2), 0.3f), "an input not chosen is measured too");
        ok (near (rig.pick.takeLevel (2), 0.0f), "a level read is cleared until the next block");
        ok (near (rig.pick.takeLevel (9), 0.0f) && near (rig.pick.takeLevel (-1), 0.0f) && near (rig.pick.takeLevel (64), 0.0f),
            "an input not open, or none at all, reads nought");
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
        const InputMemory none {};
        ok (none.recall ("Any", 4) == InputChoice::single (0) && ! none.knows ("Any"), "no store: IN 1, nothing kept");
        none.remember ("Any", InputChoice::single (1));
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

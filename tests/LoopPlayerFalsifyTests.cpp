// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// LoopPlayer plays a loop in place of a product's input. What it promises is a contract, not a sound: which files a
// folder offers and in what order; that a load either takes the whole file or leaves the player as it was; that while
// playing every sample of every channel is the loop's (a mono loop on all of them), read straight through at the same
// rate and stepped through by linear interpolation at another, round and round; that a stop rewinds; that the audio
// thread never allocates. Each is checked against numbers computed here, from the loop written to disk, not read back
// from the player.

#include <felitronics/appkit/LoopPlayer.h>

#include <atomic>
#include <bit>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

using namespace felitronics::appkit;

// Allocations counted only while armed: fill() runs on the audio thread and must not reach the allocator.
static std::atomic<bool> g_armed { false };
static std::atomic<long> g_allocs { 0 };
void* operator new (std::size_t n)
{
    if (g_armed.load (std::memory_order_relaxed)) g_allocs.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (n > 0 ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n) { return ::operator new (n); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

static int checks = 0, failures = 0;
static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}
static void group (const char* name) { std::printf ("  - %s\n", name); }
// exactly the same number, bit for bit (the loop read straight through must BE the loop)
static bool same (float a, float b) { return std::bit_cast<std::uint32_t> (a) == std::bit_cast<std::uint32_t> (b); }

// A loop written as 32-bit float WAV, so what the player decodes is exactly what was written.
static bool writeLoop (const juce::File& f, const std::vector<std::vector<float>>& chans, double rate)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out (f.createOutputStream());
    if (out == nullptr) return false;
    auto opts = juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels ((int) chans.size()).withBitsPerSample (32)
                                                .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (out, opts));
    if (w == nullptr) return false;
    juce::AudioBuffer<float> b ((int) chans.size(), (int) chans[0].size());
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (c, i, chans[(size_t) c][(size_t) i]);
    return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

// One block through fill(), every channel pre-set to a marker so an untouched sample shows.
static std::vector<std::vector<float>> run (LoopPlayer& p, int nch, int n, float marker = 7.0f)
{
    std::vector<std::vector<float>> io ((size_t) nch, std::vector<float> ((size_t) n, marker));
    std::vector<float*> ptrs; for (auto& c : io) ptrs.push_back (c.data());
    p.fill (ptrs.data(), nch, n);
    return io;
}

int main()
{
    std::printf ("LoopPlayer — a loop in place of the input\n");
    const auto dir = juce::File::createTempFile ("loopplayer");
    dir.createDirectory();

    group ("find: the loops a folder offers, in natural order, folder by folder, not below");
    {
        auto a = dir.getChildFile ("a"), b = dir.getChildFile ("b");
        a.createDirectory(); b.createDirectory(); a.getChildFile ("sub").createDirectory();
        for (auto name : { "take10.wav", "take2.wav", "take1.flac", "notes.txt", "riff.aiff" }) a.getChildFile (name).replaceWithText ("x");
        a.getChildFile ("sub").getChildFile ("deep.wav").replaceWithText ("x");
        b.getChildFile ("z.aif").replaceWithText ("x");
        const auto found = LoopPlayer::find ({ b, dir.getChildFile ("missing"), a });
        juce::StringArray names; for (const auto& f : found) names.add (f.getFileName());
        ok (names.joinIntoString (",") == "z.aif,riff.aiff,take1.flac,take2.wav,take10.wav",
            "folder order kept, natural order inside, no text, no subfolder, a missing folder skipped: got " + names.joinIntoString (",").toStdString());
        ok (LoopPlayer::find ({}).isEmpty(), "no folders, no loops");
    }

    // the loop: 64 samples, distinct values on each channel, so every read lands on a known number
    const int len = 64;
    std::vector<float> L ((size_t) len), R ((size_t) len);
    for (int i = 0; i < len; ++i) { L[(size_t) i] = (float) (i + 1) / 128.0f; R[(size_t) i] = -(float) (3 * i + 1) / 256.0f; }
    const auto stereo = dir.getChildFile ("stereo.wav"), mono = dir.getChildFile ("mono.wav"), slow = dir.getChildFile ("slow.wav");
    ok (writeLoop (stereo, { L, R }, 48000.0) && writeLoop (mono, { L }, 48000.0) && writeLoop (slow, { L, R }, 44100.0), "the fixtures are written");

    group ("load: the whole file or nothing");
    {
        LoopPlayer p; p.prepare (48000.0);
        ok (! p.load (dir.getChildFile ("absent.wav")), "a missing file does not load");
        dir.getChildFile ("text.wav").replaceWithText ("not audio");
        ok (! p.load (dir.getChildFile ("text.wav")), "a file that is not audio does not load");
        ok (p.loopName().isEmpty(), "and leaves the player empty");
        p.setPlaying (true);
        auto io = run (p, 2, 16);
        ok (same (io[0][0], 7.0f) && same (io[1][15], 7.0f), "playing with no loop leaves the input as it was");
        ok (p.load (stereo) && p.loopName() == "stereo", "a WAV loads and names itself");
        ok (! p.load (dir.getChildFile ("text.wav")) && p.loopName() == "stereo", "a failed load keeps the loop it had");
    }

    group ("fill at the loop's own rate: the loop's samples, exactly, round and round");
    {
        LoopPlayer p; p.prepare (48000.0); p.load (stereo);
        auto idle = run (p, 2, 32);
        ok (same (idle[0][0], 7.0f) && same (idle[1][31], 7.0f), "stopped: the input passes untouched");
        p.setPlaying (true);
        bool exact = true; int at = 0;
        for (int block : { 5, 64, 1, 100, 13 })   // odd blocks, across the loop's end more than once
        {
            auto io = run (p, 2, block);
            for (int i = 0; i < block; ++i, ++at)
                exact = exact && same (io[0][(size_t) i], L[(size_t) (at % len)]) && same (io[1][(size_t) i], R[(size_t) (at % len)]);
        }
        ok (exact, "left and right are the loop's own channels, sample for sample, across blocks and wraps");
        auto wide = run (p, 4, 8);
        bool lastRepeats = true;
        for (int i = 0; i < 8; ++i) lastRepeats = lastRepeats && same (wide[2][(size_t) i], wide[1][(size_t) i]) && same (wide[3][(size_t) i], wide[1][(size_t) i]);
        ok (lastRepeats, "channels past the loop's last carry its last");
    }

    group ("a mono loop plays on every channel");
    {
        LoopPlayer p; p.prepare (48000.0); p.load (mono); p.setPlaying (true);
        auto io = run (p, 2, 70);
        bool both = true;
        for (int i = 0; i < 70; ++i) both = both && same (io[0][(size_t) i], L[(size_t) (i % len)]) && same (io[1][(size_t) i], io[0][(size_t) i]);
        ok (both, "both channels are the mono loop");
    }

    group ("another rate: stepped through by linear interpolation, the last sample leaning on the first");
    {
        LoopPlayer p; p.prepare (48000.0); p.load (slow); p.setPlaying (true);
        const double step = 44100.0 / 48000.0;
        const int n = 300;   // past four wraps
        auto io = run (p, 2, n);
        double worst = 0.0, pos = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const int a = (int) pos, b = (a + 1) % len; const double t = pos - a;
            const double wantL = L[(size_t) a] + t * (L[(size_t) b] - L[(size_t) a]);
            const double wantR = R[(size_t) a] + t * (R[(size_t) b] - R[(size_t) a]);
            worst = std::max ({ worst, std::abs (io[0][(size_t) i] - wantL), std::abs (io[1][(size_t) i] - wantR) });
            pos += step; if (pos >= len) pos -= len;
        }
        ok (worst < 1.0e-6, "within float rounding of the interpolation computed here: worst " + std::to_string (worst));
        ok (same (io[0][0], L[0]), "it starts on the loop's first sample");
    }

    group ("stop rewinds, a load rewinds");
    {
        LoopPlayer p; p.prepare (48000.0); p.load (stereo); p.setPlaying (true);
        run (p, 2, 37);
        p.setPlaying (false);
        ok (! p.isPlaying(), "stopped");
        p.setPlaying (true);
        auto io = run (p, 2, 3);
        ok (same (io[0][0], L[0]) && same (io[0][2], L[2]), "playing again starts from the top");
        run (p, 2, 11);
        p.load (mono);
        io = run (p, 2, 2);
        ok (same (io[0][0], L[0]) && same (io[1][0], L[0]), "a new loop starts from its top, still playing");
    }

    group ("the audio thread does not allocate");
    {
        LoopPlayer p; p.prepare (48000.0); p.load (slow); p.setPlaying (true);
        std::vector<std::vector<float>> io (2, std::vector<float> (512, 0.0f));
        float* ptrs[2] = { io[0].data(), io[1].data() };
        g_allocs = 0; g_armed = true;
        for (int k = 0; k < 50; ++k) p.fill (ptrs, 2, 512);
        g_armed = false;
        const long allocs = g_allocs.load();
        ok (allocs == 0, "fill() made " + std::to_string (allocs) + " allocations in 50 blocks");
    }

    dir.deleteRecursively();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>

//==============================================================================
// LoopPlayer — a guitar loop played INTO a product in place of its input, so it can be heard without an instrument
// plugged in. Lifted from OrbitAmp's core/DemoPlayer (the second product that wanted it): the same contract, with the
// folders handed in by the product and the files found rather than listed.
//
//  * It REPLACES the input rather than mixing with it — a demo you can hear over your own playing is a demo you cannot
//    judge anything by.
//  * The audio is not shipped: the product names folders on the machine (`find`), and where none holds a file the
//    player and its menu are absent — the binary is the same either way.
//  * Decoding happens on the message thread when a loop is chosen; the audio thread only reads an array already there,
//    under a try-lock (a swap in progress plays the input for that block). A different rate is stepped through with
//    linear interpolation — an audition, not a sample engine.
//==============================================================================
namespace felitronics::appkit
{
class LoopPlayer
{
public:
    /** Every .wav / .aif / .aiff / .flac in the folders (not below them), in natural order, folder by folder. */
    static juce::Array<juce::File> find (const juce::Array<juce::File>& folders)
    {
        juce::Array<juce::File> out;
        for (const auto& dir : folders)
        {
            if (! dir.isDirectory()) continue;
            auto files = dir.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
            juce::StringArray names; for (const auto& f : files) names.add (f.getFullPathName());
            names.sortNatural();
            for (const auto& n : names) out.add (juce::File (n));
        }
        return out;
    }

    void prepare (double sampleRate) noexcept { rate = sampleRate > 0.0 ? sampleRate : 48000.0; position = 0.0; }

    /** Message thread. A missing or unreadable file leaves the player as it was; returns whether it loaded. */
    bool load (const juce::File& file)
    {
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > (juce::int64) 1 << 28) return false;
        juce::AudioBuffer<float> decoded ((int) juce::jmin<juce::uint32> (reader->numChannels, 2u), (int) reader->lengthInSamples);
        reader->read (&decoded, 0, (int) reader->lengthInSamples, 0, true, decoded.getNumChannels() > 1);
        {
            const juce::SpinLock::ScopedLockType lock (swapLock);
            std::swap (loop, decoded);   // the old loop is freed below, on this thread, outside the lock
            loopRate = reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;
            position = 0.0;
            name = file.getFileNameWithoutExtension();
        }
        return true;
    }

    void setPlaying (bool shouldPlay) noexcept { if (! shouldPlay) position = 0.0; playing.store (shouldPlay); }
    bool isPlaying() const noexcept { return playing.load(); }
    juce::String loopName() const { const juce::SpinLock::ScopedLockType lock (swapLock); return name; }

    /** Audio thread: the loop in place of the input while playing (every channel; a mono loop on both); else nothing. */
    void fill (float* const* io, int numChannels, int n) noexcept
    {
        if (! playing.load() || numChannels <= 0 || n <= 0) return;
        const juce::SpinLock::ScopedTryLockType lock (swapLock);
        const int len = loop.getNumSamples();
        if (! lock.isLocked() || len < 2) return;
        const double step = loopRate / rate;
        for (int i = 0; i < n; ++i)
        {
            const int a = (int) position, b = a + 1 < len ? a + 1 : 0;
            const float t = (float) (position - a);
            for (int c = 0; c < numChannels; ++c)
            {
                const int ch = juce::jmin (c, loop.getNumChannels() - 1);
                io[c][i] = loop.getSample (ch, a) + t * (loop.getSample (ch, b) - loop.getSample (ch, a));
            }
            position += step;
            if (position >= (double) len) position -= (double) len;   // round and round until stopped
        }
    }

private:
    juce::AudioBuffer<float> loop;
    mutable juce::SpinLock swapLock;
    juce::String name;
    double rate = 48000.0, loopRate = 48000.0, position = 0.0;
    std::atomic<bool> playing { false };
};
} // namespace felitronics::appkit

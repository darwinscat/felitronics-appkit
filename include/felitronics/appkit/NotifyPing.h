// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::NotifyPing — a notification chime on a SECOND output device, so a long
// unattended capture can play out of the main interface while the "take done" ping lands on e.g.
// the built-in speakers. Owns its own output-only AudioDeviceManager (multiple managers per process
// are fine — each owns one device; CoreAudio/WASAPI multiplex clients). The chime is synthesized in
// audioDeviceAboutToStart at the device's ACTUAL rate (never assume 48 k): two soft sine bursts
// with exponential decay — no asset needed. RT path only reads a pre-built buffer + atomics.
// Header-only; the consumer supplies JUCE (juce_audio_devices).
//==============================================================================

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <cmath>
#include <vector>

namespace felitronics::appkit
{

class NotifyPing : private juce::AudioIODeviceCallback
{
public:
    NotifyPing() = default;
    ~NotifyPing() override { close(); }

    // All output-capable device names across the machine's device types.
    static juce::StringArray outputDeviceNames()
    {
        juce::AudioDeviceManager probe;
        juce::StringArray out;
        for (auto* type : probe.getAvailableDeviceTypes())
        {
            type->scanForDevices();
            out.addArray (type->getDeviceNames (false));    // false = output names
        }
        out.removeDuplicates (false);
        return out;
    }

    // Open (or switch to) the named output device; empty name closes. Returns false when the
    // device refused to open — the ping then stays silent (never throws, never blocks capture).
    bool setDevice (const juce::String& outputName)
    {
        close();
        if (outputName.isEmpty()) return true;
        dm_ = std::make_unique<juce::AudioDeviceManager>();
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.outputDeviceName = outputName;
        setup.inputDeviceName  = {};
        const auto err = dm_->initialise (0, 2, nullptr, false, {}, &setup);
        if (err.isNotEmpty() || dm_->getCurrentAudioDevice() == nullptr) { dm_.reset(); return false; }
        dm_->addAudioCallback (this);
        return true;
    }

    juce::String currentDevice() const
    {
        return (dm_ != nullptr && dm_->getCurrentAudioDevice() != nullptr)
                 ? dm_->getAudioDeviceSetup().outputDeviceName : juce::String();
    }

    // Fire the chime (message thread). A ping already in flight restarts.
    void ping()
    {
        if (dm_ == nullptr) return;
        pos_.store (0, std::memory_order_relaxed);
        playing_.store (true, std::memory_order_release);
    }

    void close()
    {
        if (dm_ != nullptr) { dm_->removeAudioCallback (this); dm_.reset(); }
        playing_.store (false, std::memory_order_release);
    }

private:
    void audioDeviceAboutToStart (juce::AudioIODevice* d) override
    {
        const double sr = d->getCurrentSampleRate();
        const int n = (int) (0.55 * sr);
        chime_.assign ((size_t) n, 0.0f);
        auto burst = [&] (double t0, double freq, double amp, double decay)
        {
            const int start = (int) (t0 * sr);
            for (int i = start; i < n; ++i)
            {
                const double t = (i - start) / sr;
                chime_[(size_t) i] += (float) (amp * std::exp (-t * decay)
                                               * std::sin (2.0 * juce::MathConstants<double>::pi * freq * t));
            }
        };
        burst (0.00, 880.0,  0.28, 9.0);    // A5 …
        burst (0.16, 1318.5, 0.24, 8.0);    // … up to E6 — a friendly "done" third
        for (int i = 0; i < juce::jmin (32, n); ++i) chime_[(size_t) i] *= (float) i / 32.0f;  // click guard
    }

    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override
    {
        if (! playing_.load (std::memory_order_acquire))
        {
            for (int c = 0; c < numOut; ++c)
                if (out[c] != nullptr) juce::FloatVectorOperations::clear (out[c], numSamples);
            return;
        }
        int p = pos_.load (std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = (p < (int) chime_.size()) ? chime_[(size_t) p++] : 0.0f;
            for (int c = 0; c < numOut; ++c)
                if (out[c] != nullptr) out[c][i] = s;
        }
        pos_.store (p, std::memory_order_relaxed);
        if (p >= (int) chime_.size()) playing_.store (false, std::memory_order_release);
    }

    std::unique_ptr<juce::AudioDeviceManager> dm_;
    std::vector<float> chime_;
    std::atomic<int>  pos_ { 0 };
    std::atomic<bool> playing_ { false };
};

} // namespace felitronics::appkit

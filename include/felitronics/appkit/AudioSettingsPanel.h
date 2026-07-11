// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::AudioSettingsPanel — the shared audio-settings block every capture product
// re-wires by hand: a juce::AudioDeviceSelectorComponent + optional FORCED sample rate + device-state
// persistence. The rate enforcement re-applies on every device change and reports the actual result
// (a device can silently refuse) — the product decides what to disable when the demand isn't met.
// Header-only; the consumer supplies JUCE (juce_audio_utils).
//==============================================================================

#include <juce_audio_utils/juce_audio_utils.h>

#include <functional>

namespace felitronics::appkit
{

class AudioSettingsPanel : public juce::Component,
                           private juce::ChangeListener
{
public:
    struct Options
    {
        int    minInputs = 1, maxInputs = 32;
        int    minOutputs = 1, maxOutputs = 2;
        double forceSampleRate = 0.0;    // 0 = don't enforce
        bool   hideAdvanced = true;      // keep the rate/buffer combos away when enforcing a rate
    };

    AudioSettingsPanel (juce::AudioDeviceManager& dm, Options opts)
        : dm_ (dm), opts_ (opts),
          selector_ (dm, opts.minInputs, opts.maxInputs, opts.minOutputs, opts.maxOutputs,
                     false, false, false, opts.hideAdvanced)
    {
        addAndMakeVisible (selector_);
        dm_.addChangeListener (this);
    }

    // (A defaulted `Options opts = {}` argument is ill-formed here — a nested struct's default
    // member initializers aren't usable until the enclosing class is complete.)
    explicit AudioSettingsPanel (juce::AudioDeviceManager& dm) : AudioSettingsPanel (dm, defaults()) {}

    ~AudioSettingsPanel() override { dm_.removeChangeListener (this); }

    // Fired after every device change (and after enforcement): actual rate + whether the demand
    // (when any) is met. Also fired once right after construction wiring via enforceNow().
    std::function<void (double actualRate, bool rateOk)> onRateStatus;

    void enforceNow() { changeListenerCallback (&dm_); }

    // ---- device-state persistence (settings.json-friendly XML string) ----
    juce::String saveState() const
    {
        if (auto xml = dm_.createStateXml()) return xml->toString();
        return {};
    }

    void restoreState (const juce::String& xmlText, int defaultIns, int defaultOuts)
    {
        std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (xmlText));
        dm_.initialise (defaultIns, defaultOuts, xml.get(), true);
        enforceNow();
    }

    void resized() override { selector_.setBounds (getLocalBounds()); }

private:
    static Options defaults() { return {}; }

    // Rates are nominal values (44100/48000/96000) — a sub-Hz tolerance dodges float-equality
    // while never confusing two real rates.
    static bool sameRate (double a, double b) { return std::abs (a - b) < 0.5; }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        double actual = 0.0;
        if (auto* dev = dm_.getCurrentAudioDevice())
        {
            actual = dev->getCurrentSampleRate();
            if (opts_.forceSampleRate > 0.0 && ! sameRate (actual, opts_.forceSampleRate))
            {
                auto setup = dm_.getAudioDeviceSetup();
                setup.sampleRate = opts_.forceSampleRate;
                dm_.setAudioDeviceSetup (setup, true);                    // may silently refuse …
                if (auto* d2 = dm_.getCurrentAudioDevice())
                    actual = d2->getCurrentSampleRate();                  // … so re-read the truth
            }
        }
        if (onRateStatus)
            onRateStatus (actual, opts_.forceSampleRate <= 0.0 || sameRate (actual, opts_.forceSampleRate));
    }

    juce::AudioDeviceManager& dm_;
    Options opts_;
    juce::AudioDeviceSelectorComponent selector_;
};

} // namespace felitronics::appkit

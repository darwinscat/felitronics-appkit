// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::SettingsStore — the family's app-data settings.json: one JSON object under
// userApplicationDataDirectory/<Company>/<Product>/settings.json, read tolerantly, written via
// JUCE's temp-file swap (replaceWithText). Products keep their own keys; this owns only the file
// discipline. Header-only; the consumer supplies JUCE (juce_core / juce_data_structures via var).
//==============================================================================

#include <juce_core/juce_core.h>

namespace felitronics::appkit
{

class SettingsStore
{
public:
    SettingsStore (juce::String company, juce::String product)
        : company_ (std::move (company)), product_ (std::move (product)) {}

    juce::File file() const
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
        base = base.getChildFile ("Application Support");   // JUCE's mac value is bare ~/Library
#endif
        return base.getChildFile (company_).getChildFile (product_).getChildFile ("settings.json");
    }

    // Missing/corrupt file → an empty object (never a void var: callers chain getProperty safely).
    juce::var load() const
    {
        const auto f = file();
        if (f.existsAsFile())
            if (auto v = juce::JSON::parse (f.loadFileAsString()); v.getDynamicObject() != nullptr)
                return v;
        return juce::var (new juce::DynamicObject());
    }

    bool save (const juce::var& v) const
    {
        auto f = file();
        if (! f.getParentDirectory().createDirectory()) return false;
        return f.replaceWithText (juce::JSON::toString (v));
    }

    // Read-modify-write one key (the common case: flip a setting without touching the rest).
    bool set (const juce::Identifier& key, const juce::var& value) const
    {
        auto v = load();
        v.getDynamicObject()->setProperty (key, value);
        return save (v);
    }

    juce::var get (const juce::Identifier& key, const juce::var& fallback = {}) const
    {
        return load().getProperty (key, fallback);
    }

private:
    juce::String company_, product_;
};

} // namespace felitronics::appkit

// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

//==============================================================================
// felitronics::appkit::LicenceStore — where a paid product keeps its key and the day it first ran. juce_core only.
//
// Two plain text files in the product's own folder, the one SettingsStore uses
// (userApplicationDataDirectory/<Company>/<Product>/), so the plug-in and the standalone share them:
//
//   licence-key.txt   the accepted key block, exactly as it was pasted
//   first-run.txt     the first start's local calendar date, "YYYY-MM-DD", written on the first ask
//
// Text, not settings.json: a key is a block a person can open, copy and mail to another computer, and a date is one
// line. Both are written through a temporary file and a swap, so a reader sees the old file or the new one, never
// half of either.
//
// Nothing here guards the trial: deleting first-run.txt or setting the clock back restarts it, which costs nothing
// when the demo never ends (see LicenceTypes.h, licence::status). A first-run.txt that is not a date is rewritten
// with today, the way a missing one is written.
//==============================================================================

#include <juce_core/juce_core.h>

#include <felitronics/appkit/LicenceTypes.h>
#include <felitronics/appkit/SettingsStore.h>

#include <cstdint>
#include <optional>

namespace felitronics::appkit
{

class LicenceStore
{
public:
    LicenceStore (juce::String company, juce::String product)
        : folder_ (SettingsStore (std::move (company), std::move (product)).file().getParentDirectory()) {}

    // Test isolation, as SettingsStore's: the files go under an explicit directory instead of the user's.
    LicenceStore (juce::String company, juce::String product, juce::File baseDirOverride)
        : folder_ (SettingsStore (std::move (company), std::move (product), std::move (baseDirOverride))
                       .file().getParentDirectory()) {}

    juce::File folder() const       { return folder_; }
    juce::File keyFile() const      { return folder_.getChildFile ("licence-key.txt"); }
    juce::File firstRunFile() const { return folder_.getChildFile ("first-run.txt"); }

    // A key block is a few hundred bytes; a file far past that is not one, and is not read into memory.
    static constexpr juce::int64 kMaxKeyBytes = 64 * 1024;

    /** The stored key block, or nothing when there is none (or the file is not one: empty, too big). */
    std::optional<juce::String> key() const
    {
        const auto f = keyFile();
        if (! f.existsAsFile() || f.getSize() <= 0 || f.getSize() > kMaxKeyBytes)
            return std::nullopt;
        return f.loadFileAsString();
    }

    /** Stores an accepted key block as it was pasted. False when it could not be written. */
    bool storeKey (const juce::String& block) const { return write (keyFile(), block); }

    /** The day the product first ran here. The first ask writes `today`; a file that is not a date is rewritten
        with `today` too. */
    std::int32_t firstRunDay (std::int32_t today) const
    {
        if (const auto day = parseDate (firstRunFile()); day.has_value())
            return *day;
        write (firstRunFile(), isoDate (today) + "\n");
        return today;
    }

    std::int32_t firstRunDay() const { return firstRunDay (today()); }

    /** Today, on the local calendar: the date the user's clock shows, as a day since 1970-01-01. */
    static std::int32_t today() { return dayOf (juce::Time::getCurrentTime()); }

    static std::int32_t dayOf (const juce::Time& localTime)
    {
        return licence::dayFromCivil (localTime.getYear(), static_cast<std::uint32_t> (localTime.getMonth() + 1),
                                      static_cast<std::uint32_t> (localTime.getDayOfMonth()));
    }

    /** "2026-11-20" for a day since 1970-01-01. */
    static juce::String isoDate (std::int32_t day) { return juce::String (licence::isoDate (day)); }

    /** The day a "YYYY-MM-DD" line names (surrounding whitespace allowed), or nothing when it is not a real date. */
    static std::optional<std::int32_t> parseIsoDate (const juce::String& text)
    {
        const auto t = text.trim();
        if (t.length() != 10 || t[4] != '-' || t[7] != '-')
            return std::nullopt;
        for (int i : { 0, 1, 2, 3, 5, 6, 8, 9 })
            if (t[i] < '0' || t[i] > '9')
                return std::nullopt;
        const int y = t.substring (0, 4).getIntValue(), m = t.substring (5, 7).getIntValue(), d = t.substring (8, 10).getIntValue();
        if (y < 1970 || m < 1 || m > 12 || d < 1)
            return std::nullopt;
        const auto day = licence::dayFromCivil (y, static_cast<std::uint32_t> (m), static_cast<std::uint32_t> (d));
        const auto back = licence::civilFromDay (day);   // 2026-02-30 is no date: it does not come back as itself
        if (back.year != y || back.month != static_cast<std::uint32_t> (m) || back.day != static_cast<std::uint32_t> (d))
            return std::nullopt;
        return day;
    }

private:
    static std::optional<std::int32_t> parseDate (const juce::File& f)
    {
        if (! f.existsAsFile() || f.getSize() > 64)
            return std::nullopt;
        return parseIsoDate (f.loadFileAsString());
    }

    // The same verified swap SettingsStore writes with: the temporary file's stream is checked before it replaces
    // the target, so a full disk cannot publish half a key.
    static bool write (const juce::File& f, const juce::String& text)
    {
        if (! f.getParentDirectory().createDirectory()) return false;
        juce::TemporaryFile tmp (f);
        {
            juce::FileOutputStream os (tmp.getFile());
            if (os.failedToOpen()) return false;
            os.writeText (text, false, false, nullptr);
            os.flush();
            if (os.getStatus().failed()) return false;
        }
        return tmp.overwriteTargetFileWithTemporary();
    }

    juce::File folder_;
};

} // namespace felitronics::appkit

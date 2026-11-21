// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

//==============================================================================
// felitronics::appkit::LicenceStore — where a paid product keeps its key and the day it first ran. juce_core only.
//
// Two plain text files in the product's own folder, the one SettingsStore uses
// (userApplicationDataDirectory/<Company>/<Product>/), so the plug-in and the standalone share them:
//
//   licence-key.txt   the accepted key, alone, as it is issued (Reading::block) — never the letter it came in, so no
//                     e-mail text lands on disk and the file stays a few hundred bytes
//   first-run.txt     the first start's local calendar date, "YYYY-MM-DD", written on the first ask
//
// Text, not settings.json: a key is a block a person can open, copy and mail to another computer, and a date is one
// line. Both are written to a temporary file that is read back before it takes the name — a short write (a full disk,
// a file-size limit) fails instead of publishing half a key — so a reader sees the old file or the new one, never half
// of either. The first run's file is created exclusively: when two instances start at once, the first writer wins and
// the other reads its date.
//
// Nothing here guards the trial: deleting first-run.txt or setting the clock back restarts it, which costs nothing
// when the demo never ends (see LicenceTypes.h, licence::status). A first-run.txt that is not a date is rewritten
// with today, the way a missing one is written. A date before 2026-01-01 (a clock whose battery died) reads as
// 2026-01-01: no trial of these products began earlier.
//==============================================================================

#include <juce_core/juce_core.h>

#include <felitronics/appkit/LicenceTypes.h>
#include <felitronics/appkit/SettingsStore.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <system_error>

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
    static constexpr juce::int64 kMaxKeyBytes = 4096;

    // The earliest first run there can be: the day these products' trials began to exist.
    static constexpr std::int32_t kEarliestFirstRun = licence::dayFromCivil (2026, 1, 1);

    /** The stored key block, or nothing when there is none (or the file is not one: empty, too big). */
    std::optional<juce::String> key() const
    {
        const auto text = readWhole (keyFile(), kMaxKeyBytes);
        if (! text.has_value() || text->isEmpty())
            return std::nullopt;
        return text;
    }

    /** Stores an accepted key: the valid reading's canonical block, never the paste. False for anything but a valid
        reading, and when it could not be written whole. */
    bool storeKey (const licence::Reading& accepted) const
    {
        if (accepted.verdict != licence::Verdict::valid || accepted.block.empty())
            return false;
        return write (keyFile(), juce::String::fromUTF8 (accepted.block.c_str()));
    }

    /** The day the product first ran here. The first ask writes `today`; a file that is not a date is rewritten
        with `today` too. */
    std::int32_t firstRunDay (std::int32_t today) const
    {
        const auto f = firstRunFile();
        const auto stored = [&]() -> std::optional<std::int32_t>
        {
            if (const auto d = parseDate (f); d.has_value())
                return juce::jmax (*d, kEarliestFirstRun);
            return std::nullopt;
        };
        if (const auto d = stored(); d.has_value())
            return *d;

        const auto day = juce::jmax (today, kEarliestFirstRun);
        const auto line = isoDate (day) + "\n";
        if (createExclusive (f, line))   // nobody had written it: ours
            return day;
        if (const auto d = stored(); d.has_value())   // another instance was first (or it was there all along): theirs
            return *d;
        write (f, line);   // there, but not a date (or nothing could be written): written over
        return day;
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
        auto t = text.trim();
        if (t.isNotEmpty() && t[0] == 0xfeff)   // a byte order mark (Notepad writes one)
            t = t.substring (1).trim();
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
        const auto text = readWhole (f, 64);
        return text.has_value() ? parseIsoDate (*text) : std::nullopt;
    }

    // The whole of the file that was opened, read to its end — never to a length measured by name a moment before:
    // a swap in between would cut the read at the old file's length, half of the new one. Nothing past `maxBytes`.
    static std::optional<juce::String> readWhole (const juce::File& f, juce::int64 maxBytes)
    {
        juce::FileInputStream in (f);
        if (! in.openedOk())
            return std::nullopt;
        juce::MemoryBlock all;
        char chunk[512];
        for (;;)
        {
            const int n = in.read (chunk, (int) sizeof (chunk));
            if (n <= 0)
                break;
            all.append (chunk, (size_t) n);
            if ((juce::int64) all.getSize() > maxBytes)
                return std::nullopt;
        }
        return juce::String::fromUTF8 (static_cast<const char*> (all.getData()), (int) all.getSize());
    }

    // A temporary file beside the target, named so that no other writer — another thread, the plug-in and the
    // standalone at once — picks the same name (JUCE's TemporaryFile draws its name from a shared, unlocked Random).
    struct Temp
    {
        explicit Temp (const juce::File& target)
            : file (target.getSiblingFile (target.getFileName() + "." + tag() + ".tmp")) {}
        ~Temp() { file.deleteFile(); }

        static juce::String tag()
        {
            static std::atomic<std::uint32_t> count { 0 };
            return juce::String::toHexString (juce::Time::getHighResolutionTicks()) + "-"
                 + juce::String::toHexString ((juce::int64) ++count) + "-" + juce::String::toHexString (juce::Random().nextInt64());
        }

        juce::File file;
        JUCE_DECLARE_NON_COPYABLE (Temp)
    };

    // Writes `text` to `f` and reads it back: the bytes on disk must be exactly the text's. JUCE's stream reports a
    // failed write but not a SHORT one (a file-size limit, a full disk part way through), so the file is measured.
    static bool writeChecked (const juce::File& f, const juce::String& text)
    {
        const char* utf8 = text.toRawUTF8();
        const auto bytes = text.getNumBytesAsUTF8();
        {
            juce::FileOutputStream os (f);
            if (os.failedToOpen() || ! os.setPosition (0) || os.truncate().failed()) return false;
            if (bytes > 0 && ! os.write (utf8, bytes)) return false;
            os.flush();
            if (os.getStatus().failed()) return false;
        }
        const auto back = readWhole (f, (juce::int64) bytes);
        return back.has_value() && back->getNumBytesAsUTF8() == bytes && std::memcmp (back->toRawUTF8(), utf8, bytes) == 0;
    }

    // The swap: a checked temporary file takes the name, replacing what was there in one rename (a few tries: on
    // Windows a reader holding the file open makes the rename wait).
    static bool write (const juce::File& f, const juce::String& text)
    {
        if (! ensureFolder (f)) return false;
        const Temp tmp (f);
        if (! writeChecked (tmp.file, text)) return false;
        for (int attempt = 0; attempt < 5; ++attempt)
        {
            if (tmp.file.replaceFileIn (f))
                return true;
            juce::Thread::sleep (20);
        }
        return false;
    }

    // The folder, made if it is not there. Two instances making it at once both succeed: the one whose mkdir lost the
    // race finds the folder there, which is all it wanted.
    static bool ensureFolder (const juce::File& f)
    {
        const auto dir = f.getParentDirectory();
        return dir.createDirectory().wasOk() || dir.isDirectory();
    }

    static std::filesystem::path pathOf (const juce::File& f)
    {
       #if JUCE_WINDOWS
        return std::filesystem::path (f.getFullPathName().toWideCharPointer());
       #else
        return std::filesystem::path (f.getFullPathName().toStdString());
       #endif
    }

    // The first writer wins: a checked temporary file is hard-linked to the name, which the system refuses when the
    // name already exists. True when this call created the file. On a file system without hard links (FAT) the swap
    // is the next best thing.
    static bool createExclusive (const juce::File& f, const juce::String& text)
    {
        if (! ensureFolder (f)) return false;
        const Temp tmp (f);
        if (! writeChecked (tmp.file, text)) return false;
        std::error_code ec;
        std::filesystem::create_hard_link (pathOf (tmp.file), pathOf (f), ec);
        if (! ec) return true;   // the temporary's own name goes with `tmp`; the file stays under ours
        if (f.exists()) return false;
        return tmp.file.replaceFileIn (f);
    }

    juce::File folder_;
};

} // namespace felitronics::appkit

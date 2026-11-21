// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// LicenceStore.h: the key comes back byte for byte, first-run.txt is written once and then stays, a file that is not
// a date is rewritten, the day arithmetic agrees with std::chrono's calendar on every 16-bit day, and a store that
// cannot write fails without falling over. Every file lives in a fresh temporary folder.

#include <juce_core/juce_core.h>
#include <felitronics/appkit/LicenceStore.h>
#include <felitronics/appkit/SettingsStore.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

using namespace felitronics::appkit;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

// The oracle for the calendar: std::chrono's, which shares no code with Hinnant's formulas as LicenceTypes.h has them.
static std::int32_t chronoDay (int y, unsigned m, unsigned d)
{
    using namespace std::chrono;
    return static_cast<std::int32_t> (sys_days (year (y) / month (m) / day (d)).time_since_epoch().count());
}

static juce::String bytesOf (const juce::File& f)
{
    juce::MemoryBlock mb;
    f.loadFileAsData (mb);
    return juce::String::fromUTF8 (static_cast<const char*> (mb.getData()), static_cast<int> (mb.getSize()));
}

int main()
{
    std::printf ("felitronics::appkit licence store tests\n");
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("appkit-licence-store-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
    ok (tmp.createDirectory().wasOk(), "a fresh temporary folder");

    group ("the folder is SettingsStore's, the files are plain text beside settings.json");
    {
        const LicenceStore s ("Darwin's Cat", "Orbit480", tmp);
        const SettingsStore settings ("Darwin's Cat", "Orbit480", tmp);
        ok (s.folder() == settings.file().getParentDirectory(), "the same folder as settings.json");
        ok (s.keyFile().getFileName() == "licence-key.txt" && s.keyFile().getParentDirectory() == s.folder(), "licence-key.txt");
        ok (s.firstRunFile().getFileName() == "first-run.txt" && s.firstRunFile().getParentDirectory() == s.folder(), "first-run.txt");
        const LicenceStore user ("Darwin's Cat", "Orbit480");
        ok (user.folder() == SettingsStore ("Darwin's Cat", "Orbit480").file().getParentDirectory(),
            "without an override: the user's own SettingsStore folder");
    }

    group ("the key: stored as pasted, read back byte for byte, shared by plug-in and standalone");
    {
        const LicenceStore plugin ("Darwin's Cat", "Orbit480", tmp), standalone ("Darwin's Cat", "Orbit480", tmp);
        ok (! plugin.key().has_value(), "no key before one is stored");
        const auto block = juce::String::fromUTF8 ("Thanks for buying, \xc3\x89lodie!\r\n> -----BEGIN ORBIT480 KEY-----\r\n"
                                                   ">  IRXU AAAA\n-----END ORBIT480 KEY-----\n\n");
        ok (plugin.storeKey (block), "storeKey writes");
        ok (plugin.key().has_value() && *plugin.key() == block, "the block comes back as it was pasted");
        ok (bytesOf (plugin.keyFile()) == block, "the file holds exactly the block's UTF-8: no mark, no newline added");
        ok (standalone.key().has_value() && *standalone.key() == block, "the other format's store reads the same key");
        ok (plugin.storeKey ("second"), "a second key replaces the first");
        ok (*standalone.key() == "second", "...and the other store sees the replacement");
        ok (plugin.folder().findChildFiles (juce::File::findFiles, false, "*.tmp").isEmpty()
                && plugin.folder().getNumberOfChildFiles (juce::File::findFiles) == 1,
            "the swap leaves no temporary file behind");

        plugin.keyFile().replaceWithText ({});
        ok (! plugin.key().has_value(), "an empty key file is no key");
        juce::MemoryBlock big (static_cast<size_t> (LicenceStore::kMaxKeyBytes + 1), true);
        plugin.keyFile().replaceWithData (big.getData(), big.getSize());
        ok (! plugin.key().has_value(), "a file past 64 KiB is not read as a key");
    }

    group ("first-run.txt: written on the first ask, then stable");
    {
        const LicenceStore s ("Darwin's Cat", "FirstRun", tmp);
        const auto day = chronoDay (2026, 11, 20);
        ok (day == 20777, "the oracle: 2026-11-20 is day 20777");
        ok (! s.firstRunFile().exists(), "no file before the first ask");
        ok (s.firstRunDay (day) == day, "the first ask is today");
        ok (bytesOf (s.firstRunFile()) == "2026-11-20\n", "the file is the local date, YYYY-MM-DD and a newline");
        const auto written = s.firstRunFile().getLastModificationTime();
        ok (s.firstRunDay (day + 5) == day && s.firstRunDay (day + 400) == day, "later asks keep the first day");
        ok (bytesOf (s.firstRunFile()) == "2026-11-20\n" && s.firstRunFile().getLastModificationTime() == written,
            "...and leave the file alone");
        ok (LicenceStore ("Darwin's Cat", "FirstRun", tmp).firstRunDay (day + 9) == day, "a second store reads the same day");
        ok (s.firstRunDay (day - 30) == day, "a clock set back does not move it (status() reads that as day one)");
    }

    group ("first-run.txt that is not a date: rewritten with today");
    {
        const LicenceStore s ("Darwin's Cat", "Malformed", tmp);
        const auto today = chronoDay (2027, 3, 4);
        for (const char* bad : { "2026-13-01", "garbage", "", "2026-02-30", "2025-02-29", "20261120", "2026-11-20x",
                                 "2026/11/20", "1969-12-31", "2026-11-2", "+026-11-20", "2026-00-10", "2026-11-00",
                                 "2026-11-20\n2026-11-21" })
        {
            s.folder().createDirectory();
            s.firstRunFile().replaceWithText (juce::String::fromUTF8 (bad));
            ok (s.firstRunDay (today) == today, std::string ("\"") + bad + "\" is no date: today");
            ok (bytesOf (s.firstRunFile()) == "2027-03-04\n", std::string ("\"") + bad + "\" is replaced by today's date");
        }
        s.firstRunFile().replaceWithText (juce::String::fromUTF8 ("\xef\xbb\xbf" "2026-11-20"));
        ok (s.firstRunDay (today) == chronoDay (2026, 11, 20), "a date saved with a byte order mark (Notepad) is the date");
        s.firstRunFile().replaceWithText ("  2024-02-29 \r\n");
        ok (s.firstRunDay (today) == chronoDay (2024, 2, 29), "whitespace around a real date (a leap day) is still the date");
        juce::String longFile = "2026-11-20";
        longFile << juce::String::repeatedString (" ", 80);
        s.firstRunFile().replaceWithText (longFile);
        ok (s.firstRunDay (today) == today, "a file far longer than a date is not one");
    }

    group ("the calendar: dayFromCivil, civilFromDay and isoDate against std::chrono, on every 16-bit day");
    {
        int bad = 0;
        for (std::int32_t d = 0; d <= 0xffff; ++d)
        {
            const auto c = licence::civilFromDay (d);
            using namespace std::chrono;
            const year_month_day ymd { sys_days (days (d)) };
            const bool same = c.year == static_cast<int> (ymd.year()) && c.month == static_cast<unsigned> (ymd.month())
                           && c.day == static_cast<unsigned> (ymd.day())
                           && licence::dayFromCivil (c.year, c.month, c.day) == d
                           && LicenceStore::parseIsoDate (LicenceStore::isoDate (d)) == d;
            if (! same && ++bad <= 3)
                ok (false, "day " + std::to_string (d));
        }
        ok (bad == 0, "65536 days agree (" + std::to_string (bad) + " disagree)");
        ok (LicenceStore::isoDate (0) == "1970-01-01" && LicenceStore::isoDate (0xffff) == "2149-06-06", "the two ends of 16 bits");
    }

    group ("today: the local calendar date");
    {
        const auto t = std::time (nullptr);
        std::tm local {};
       #if defined (_WIN32)
        localtime_s (&local, &t);
       #else
        localtime_r (&t, &local);
       #endif
        const auto oracle = chronoDay (local.tm_year + 1900, static_cast<unsigned> (local.tm_mon + 1), static_cast<unsigned> (local.tm_mday));
        const auto got = LicenceStore::today();
        ok (got == oracle || got == oracle + 1, "today() is the C library's local date (or the next, past midnight)");
        ok (LicenceStore::dayOf (juce::Time (2026, 10, 20, 23, 59, 0, 0, true)) == 20777
                && LicenceStore::dayOf (juce::Time (2026, 10, 21, 0, 0, 0, 0, true)) == 20778,
            "dayOf reads the local wall clock: 23:59 on the 20th is the 20th");
    }

    group ("a store that cannot write fails, and does not fall over");
    {
        const auto blocker = tmp.getChildFile ("blocker");
        blocker.replaceWithText ("a file where the folder would be");
        const LicenceStore s ("Darwin's Cat", "Orbit480", blocker);
        ok (! s.storeKey ("key"), "storeKey says false");
        ok (! s.key().has_value(), "...and there is no key");
        ok (s.firstRunDay (77) == 77, "firstRunDay still answers today");
    }

    tmp.deleteRecursively();
    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

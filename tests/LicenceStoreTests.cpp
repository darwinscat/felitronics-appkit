// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// LicenceStore.h: only an accepted key is stored, and only the key itself (its canonical block, never the letter);
// a short write fails and leaves the old file; a reader never sees half a key while a writer swaps; first-run.txt is
// written once, by the first of several instances starting together, and then stays; a date that is not one is
// rewritten; a date before 2026 counts from 2026-01-01; the day arithmetic agrees with std::chrono's calendar on every
// 16-bit day; a store that cannot write fails without falling over. Every file lives in a fresh temporary folder.

#include <juce_core/juce_core.h>
#include <felitronics/appkit/LicenceStore.h>
#include <felitronics/appkit/SettingsStore.h>
#include <felitronics/appkit/LicenceKeyIssue.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>
#include <vector>
#if ! defined (_WIN32)
 #include <sys/resource.h>
#endif
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

static const licence::Seed kTestSeed = [] {
    licence::Seed s {};
    const char* hex = "c6cd52fd8cc22c76f043df0948ed0ff5d94fb84a9264ebea04e6b321f86fbf9b";
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<std::uint8_t> (juce::String (hex + 2 * i, 2).getHexValue32());
    return s;
}();
static const licence::PublicKey kPub = licence::publicKeyOf (kTestSeed);

static licence::Key ivan()
{
    licence::Key k;
    k.product = "orbit480"; k.name = "Ivan Petrov"; k.email = "ivan@example.com"; k.number = 42;
    k.issued = 20777; k.updatesUntil = 20777 + 365;
    return k;
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

    group ("the key: the accepted key alone, as issued, shared by plug-in and standalone");
    {
        const LicenceStore plugin ("Darwin's Cat", "Orbit480", tmp), standalone ("Darwin's Cat", "Orbit480", tmp);
        ok (! plugin.key().has_value(), "no key before one is stored");
        const auto issued = *licence::issue (ivan(), kTestSeed);
        const std::string letter = "Thanks for buying, \xc3\x89lodie!\r\n\r\n" + issued + "\r\nCheers, and see you at the gig\r\n";
        const auto accepted = licence::read (letter, kPub, "orbit480");
        ok (accepted.verdict == licence::Verdict::valid && plugin.storeKey (accepted), "storeKey writes a valid reading");
        ok (bytesOf (plugin.keyFile()) == juce::String (issued), "the file is the key alone, exactly as issued: no letter, no e-mail text");
        ok (plugin.key().has_value() && *plugin.key() == juce::String (issued), "...and reads back as that");
        ok (licence::read (plugin.key()->toStdString(), kPub, "orbit480").verdict == licence::Verdict::valid, "...which reads as the key");
        ok (standalone.key().has_value() && *standalone.key() == *plugin.key(), "the other format's store reads the same key");

        auto amp = ivan(); amp.product = "orbit-amp";
        const auto other = licence::read (*licence::issue (amp, kTestSeed), kPub, "orbit480");
        auto damaged = accepted; damaged.verdict = licence::Verdict::damaged;
        ok (! plugin.storeKey (other) && ! plugin.storeKey (damaged) && ! plugin.storeKey (licence::Reading {}),
            "another product's key, a damaged one, an empty reading: not stored");
        ok (*plugin.key() == juce::String (issued), "...and the stored key stays");

        auto second = ivan(); second.number = 43;
        ok (plugin.storeKey (licence::read (*licence::issue (second, kTestSeed), kPub, "orbit480")), "a second key replaces the first");
        ok (*standalone.key() == juce::String (*licence::issue (second, kTestSeed)), "...and the other store sees the replacement");
        ok (plugin.folder().findChildFiles (juce::File::findFiles, false, "*.tmp").isEmpty()
                && plugin.folder().getNumberOfChildFiles (juce::File::findFiles) == 1,
            "the swap leaves no temporary file behind");

        plugin.keyFile().replaceWithText ({});
        ok (! plugin.key().has_value(), "an empty key file is no key");
        juce::MemoryBlock big (static_cast<size_t> (LicenceStore::kMaxKeyBytes + 1), true);
        plugin.keyFile().replaceWithData (big.getData(), big.getSize());
        ok (! plugin.key().has_value(), "a file past 4 KiB is not read as a key");
    }

   #if ! defined (_WIN32)
    group ("a short write fails and leaves the old key (a file-size limit, as a full disk would)");
    {
        const LicenceStore s ("Darwin's Cat", "ShortWrite", tmp);
        const auto first = licence::read (*licence::issue (ivan(), kTestSeed), kPub, "orbit480");
        auto k2 = ivan(); k2.number = 77; k2.name = "A much longer name, so the second block is longer than the limit";
        const auto second = licence::read (*licence::issue (k2, kTestSeed), kPub, "orbit480");
        ok (s.storeKey (first), "the first key is stored");
        const auto before = bytesOf (s.keyFile());

        std::signal (SIGXFSZ, SIG_IGN);   // past the limit, write() says EFBIG instead of the signal ending the test
        rlimit old {};
        getrlimit (RLIMIT_FSIZE, &old);
        rlimit cap = old;
        cap.rlim_cur = 200;   // shorter than either block: the write is cut short, not refused
        setrlimit (RLIMIT_FSIZE, &cap);
        const bool stored = s.storeKey (second);
        const bool exclusive = LicenceStore ("Darwin's Cat", "ShortWriteFirstRun", tmp).firstRunDay (20777) == 20777;
        setrlimit (RLIMIT_FSIZE, &old);

        ok (! stored, "the cut-short write says false");
        ok (bytesOf (s.keyFile()) == before, "...and the stored key is the old one, whole");
        ok (s.folder().getNumberOfChildFiles (juce::File::findFiles) == 1, "...and no temporary file is left");
        ok (exclusive, "a first-run line that fits under the limit is still written");
    }
   #endif

    group ("never half of either: a reader while a writer swaps two keys");
    {
        const LicenceStore s ("Darwin's Cat", "Swap", tmp);
        auto kb = ivan(); kb.number = 99; kb.name = "Someone with a rather longer name than Ivan";
        const auto a = licence::read (*licence::issue (ivan(), kTestSeed), kPub, "orbit480");
        const auto b = licence::read (*licence::issue (kb, kTestSeed), kPub, "orbit480");
        const juce::String ta (a.block), tb (b.block);
        ok (s.storeKey (a), "the first key is there before the reader starts");
        std::atomic<bool> done { false };
        std::atomic<int> writes { 0 };
        std::thread writer ([&]
        {
            for (int i = 0; i < 300; ++i)
                if (s.storeKey (i % 2 == 0 ? b : a)) ++writes;
            done = true;
        });
        int reads = 0, whole = 0, half = 0, none = 0;
        while (! done || reads < 50)
        {
            const auto k = s.key();
            ++reads;
            if (! k.has_value())               ++none;
            else if (*k == ta || *k == tb)    ++whole;
            else                               ++half;
        }
        writer.join();
        ok (half == 0, "no read saw part of a key (" + std::to_string (reads) + " reads, " + std::to_string (writes.load())
                           + " writes, " + std::to_string (none) + " found the file busy)");
       #if ! defined (_WIN32)
        ok (none == 0 && writes == 300, "on POSIX the name always holds one whole key, and every swap succeeds");
       #endif
        ok (whole > 0 && writes > 0, "both sides did work");
    }

    group ("first-run.txt: the first writer wins when instances start together");
    {
        int disagreements = 0;
        for (int round = 0; round < 20; ++round)
        {
            const auto product = "Race" + juce::String (round);
            std::vector<std::int32_t> got (8, 0);
            std::atomic<int> ready { 0 };
            std::vector<std::thread> threads;
            for (int i = 0; i < 8; ++i)
                threads.emplace_back ([&, i]
                {
                    const LicenceStore s ("Darwin's Cat", product, tmp);
                    ++ready;
                    while (ready < 8) std::this_thread::yield();
                    got[(size_t) i] = s.firstRunDay (20777 + i);   // each instance believes another day
                });
            for (auto& th : threads) th.join();
            const auto file = LicenceStore ("Darwin's Cat", product, tmp).firstRunDay (30000);
            for (auto d : got)
                if (d != file) ++disagreements;
        }
        ok (disagreements == 0, "every instance answers the date the file kept (" + std::to_string (disagreements) + " did not, of 160)");
    }

    group ("a first run before 2026 (a clock with a dead battery) counts from 2026-01-01");
    {
        const LicenceStore s ("Darwin's Cat", "DeadBattery", tmp);
        const auto floor = chronoDay (2026, 1, 1);
        ok (LicenceStore::kEarliestFirstRun == floor, "the floor is 2026-01-01");
        ok (s.firstRunDay (0) == floor && bytesOf (s.firstRunFile()) == "2026-01-01\n", "a clock at 1970: day one is 2026-01-01, written so");
        s.firstRunFile().replaceWithText ("2001-05-05\n");
        ok (s.firstRunDay (chronoDay (2027, 1, 1)) == floor, "a stored 2001-05-05 reads as 2026-01-01");
        s.firstRunFile().replaceWithText ("2025-12-31\n");
        ok (s.firstRunDay (chronoDay (2027, 1, 1)) == floor, "the last day of 2025 too");
        s.firstRunFile().replaceWithText ("2026-01-02\n");
        ok (s.firstRunDay (chronoDay (2027, 1, 1)) == floor + 1, "2026-01-02 is itself");
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
        s.firstRunFile().replaceWithText ("  2028-02-29 \r\n");
        ok (s.firstRunDay (today) == chronoDay (2028, 2, 29), "whitespace around a real date (a leap day) is still the date");
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
        ok (! s.storeKey (licence::read (*licence::issue (ivan(), kTestSeed), kPub, "orbit480")), "storeKey says false");
        ok (! s.key().has_value(), "...and there is no key");
        ok (s.firstRunDay (20800) == 20800, "firstRunDay still answers today");
    }

    tmp.deleteRecursively();
    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

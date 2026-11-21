// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

// LicenceTypes.h — what a key says and where a product stands, without the crypto that checks it: plain C++, no JUCE,
// no Monocypher. A product's screens (the About window's licence block, the after-trial card, the store) include this;
// only the code that reads a pasted key includes LicenceKey.h and links felitronics::appkit_licence.
//
// Days are whole days since 1970-01-01 (UTC), as 16 bits: enough until 2149. kForever (0xffff) covers every version.

#include <cstdint>
#include <optional>
#include <string>

namespace felitronics::appkit::licence
{
inline constexpr std::uint16_t kForever = 0xffff;

// Why the key was given. Numbers are fixed once issued; a product shows an unknown one as a plain licence.
namespace kind
{
inline constexpr std::uint8_t bought = 0, student = 1, musician = 2, link = 3, music = 4, gift = 5;
}

struct Key
{
    std::string   product;             // the product's id: [a-z0-9-], 1..32 characters, e.g. "orbit480"
    std::string   name;                // UTF-8, up to 120 bytes (may be empty)
    std::string   email;               // UTF-8, up to 120 bytes (may be empty)
    std::uint32_t number       = 0;    // the key's number
    std::uint16_t issued       = 0;    // the purchase (or gift) day
    std::uint16_t updatesUntil = 0;    // the last release day covered; kForever = every version
    std::uint8_t  kind         = kind::bought;

    bool operator== (const Key&) const = default;
};

enum class Verdict
{
    valid,          // signed by us, for this product
    damaged,        // anything else: not a key, cut short, a character changed, signed by someone else
    otherProduct,   // a genuine key for another product (Reading::key says which)
    newerFormat     // a genuine key in a format this build does not know yet: a newer build reads it
};

struct Reading
{
    Verdict verdict = Verdict::damaged;
    Key     key;    // filled for valid and otherProduct

    // Valid and otherProduct: the key alone, written out as it is issued (BEGIN line, base32, END line) — what a
    // product stores, never the letter it was pasted from.
    std::string block;
};

/** Does the key cover a version released on `releaseDay`? */
inline bool covers (const Key& k, std::uint16_t releaseDay) noexcept
{
    return k.updatesUntil == kForever || releaseDay <= k.updatesUntil;
}

/** Days since 1970-01-01 for a civil date (proleptic Gregorian; H. Hinnant's days_from_civil). */
constexpr std::int32_t dayFromCivil (std::int32_t y, std::uint32_t m, std::uint32_t d) noexcept
{
    y -= m <= 2 ? 1 : 0;
    const std::int32_t era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<std::uint32_t> (y - era * 400);
    const std::uint32_t doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const std::uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int32_t> (doe) - 719468;
}

/** The civil date of a day since 1970-01-01: dayFromCivil's inverse (H. Hinnant's civil_from_days). */
struct Civil
{
    std::int32_t  year  = 1970;
    std::uint32_t month = 1;   // 1..12
    std::uint32_t day   = 1;   // 1..31
};

constexpr Civil civilFromDay (std::int32_t z) noexcept
{
    z += 719468;
    const std::int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    const auto doe = static_cast<std::uint32_t> (z - era * 146097);
    const std::uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::uint32_t mp  = (5 * doy + 2) / 153;
    const std::uint32_t d   = doy - (153 * mp + 2) / 5 + 1;
    const std::uint32_t m   = mp < 10 ? mp + 3 : mp - 9;
    return { static_cast<std::int32_t> (yoe) + era * 400 + (m <= 2 ? 1 : 0), m, d };
}

/** "2026-11-20" for a day since 1970-01-01: the one date format the licence screens and files use, whatever the
    language (a date written out in words would be one more thing to translate, and one more to misread). */
inline std::string isoDate (std::int32_t day)
{
    const auto c = civilFromDay (day);
    auto two = [] (std::uint32_t v) { return std::string { static_cast<char> ('0' + v / 10), static_cast<char> ('0' + v % 10) }; };
    std::string year = std::to_string (c.year);
    while (year.size() < 4) year.insert (0, "0");
    return year + "-" + two (c.month) + "-" + two (c.day);
}

/** Where a product stands: in the trial, the demo after it, licensed, or licensed for older versions only. */
struct Status
{
    enum class State { trial, demo, licensed, updatesOver };
    State state = State::trial;
    int   trialDaysLeft = 0;   // trial only: 1..trialDays
};

/** `key` is a valid key for this product (or none); `firstRun` and `today` are days on this computer's clock;
    `releaseDay` is this build's release day. Without a key: the trial, then the demo. A key whose updates ended
    before this release: `updatesOver` at once, which the product shows as the demo plus the renewal offer. A clock
    set before the first run reads as the first day. */
inline Status status (const std::optional<Key>& key, std::int32_t firstRun, std::int32_t today, std::uint16_t releaseDay,
                      int trialDays = 30) noexcept
{
    if (key && covers (*key, releaseDay)) return { Status::State::licensed, 0 };
    const auto used = today < firstRun ? 0 : today - firstRun;
    if (used < trialDays && ! key) return { Status::State::trial, trialDays - used };
    return { key ? Status::State::updatesOver : Status::State::demo, 0 };
}
} // namespace felitronics::appkit::licence

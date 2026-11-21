// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

// English: the keys. Every sentence the About window, its licence pages and the after-trial card say, exactly as the
// code passes it to tr(). A table for another language translates each of these once and nothing else (the i18n
// test holds every table to that). Placeholders in braces are filled after the lookup and stay as they are.

#include <felitronics/appkit/i18n/Types.h>

namespace felitronics::appkit::i18n::tables
{
inline constexpr const char8_t* en[] {
    // VersionBadge: the corner badge and the About window
    u8"{product} {version} ({format}) — click to check for updates",
    u8"Check for updates",
    u8"Automatically, once a day",
    u8"Asks GitHub for the latest release. Nothing is sent to us.",
    u8"The request goes to github.com, which sees your IP address and this product's name and version. Nothing reaches the maker, and the versions are compared here on your machine.",
    u8"Like the app?",
    u8"Close",
    u8"Copy build stamp",
    u8"copied ✓",
    u8"Checking…",
    u8"Couldn’t check (offline?)",
    u8"✓ Up to date",
    u8"↑ Update available: v{version}  —  Download",
    u8"Latest release: v{version}  —  Download",
    u8"Download",

    // The licence block in a paid product's About window
    u8"Licence",
    u8"Everything works. After the trial it keeps working too.",
    u8"Not licensed",
    u8"The trial is over. Nothing is switched off and the sound stays the same.",
    u8"Licensed to {name}",
    u8"Licensed",
    u8"key {number}",
    u8"Thank you.",
    u8"Updates ended {date}",
    u8"This version came out after your year of updates. It keeps working as the demo.",
    u8"Renew at 50 % off",
    u8"Buy {product}",
    u8"Enter key",
    u8"Free licence for students and musicians",

    // The page that takes a key
    u8"Back",
    u8"Enter your key",
    u8"Paste the whole block from the e-mail, or drop the key file here. No internet needed.",
    u8"Activate",
    u8"Licensed to {name}. Thank you.",
    u8"Licensed. Thank you.",
    u8"This key is for {other}, not {product}.",
    u8"This key needs a newer version of {product}.",
    u8"This key is damaged: paste the whole block again.",

    // The page that asks for a free licence, and the letter it writes
    u8"Ask for a free licence",
    u8"Tell us which one is you. We answer with a key, usually within a day or two.",
    u8"I am",
    u8"a student",
    u8"a musician short on money",
    u8"someone who posted a link to {product}",
    u8"sending you my music",
    u8"Your name",
    u8"E-mail for the key",
    u8"A link: your school, your post or your music (optional)",
    u8"A few words (optional)",
    u8"Write the e-mail",
    u8"Opens your mail app with this filled in. Nothing is sent from {product} itself.",
    u8"Free licence for {product}",
    u8"Name",
    u8"E-mail",
    u8"Link",

    // The card over the product's window after the trial
    u8"{product} keeps playing",
    u8"Your trial is over. Nothing is switched off and the sound stays the same. If {product} has earned its place in your mixes, please buy it.",
    u8"Buy {product} · {price}",
    u8"Can’t pay? We send a free licence when",
    u8"you are a student",
    u8"you are a musician short on money",
    u8"you post a link to {product} anywhere and tell us where",
    u8"you send us your music: if we like it (we will)",
    u8"Not now",
    u8"Not now · {n}",
};

// Counted phrases: the English forms (CLDR: one for 1, other for everything else). The key is the `other` form.
inline constexpr Counted enCounted[] {
    { u8"Trial · {n} days left", Plural::one,   u8"Trial · {n} day left" },
    { u8"Trial · {n} days left", Plural::other, u8"Trial · {n} days left" },
};
} // namespace felitronics::appkit::i18n::tables

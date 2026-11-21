// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

//==============================================================================
// felitronics::appkit::i18n — the words appkit's own windows say, in the twelve languages of darwinscat.com:
// en de uk ru cs es fr it pl pt ro tr. juce_core only.
//
//   tr ("Check for updates")            the current language's sentence; the English key itself when there is none
//   trCount ("Trial · {n} days left", 5) a counted phrase in the form CLDR's plural rules pick for 5, {n} filled in
//   fill (text, { { "name", n } })      the placeholders, filled after the lookup (so a value is never translated)
//
// The language is the system's display language (the part before '-' or '_': "pt-BR" is "pt"), English when it is
// not one of the twelve; setLanguage ("de") overrides it (tests, screenshots, a product's own language menu) and
// setLanguage ({}) goes back to the system's. Message thread only, like everything that draws.
//
// Local on purpose: nothing here touches juce::LocalisedStrings. A product's own strings, and JUCE's, keep whatever
// mapping the product installed (or none) — a library that swapped the global table would change them behind its back.
//
// The tables are C++ headers in i18n/, one per language, keyed by the English sentence (i18n/en.h lists the keys).
// Names, versions, URLs and the dependency table are never looked up.
//==============================================================================

#include <juce_core/juce_core.h>

#include <felitronics/appkit/i18n/Types.h>
#include <felitronics/appkit/i18n/en.h>
#include <felitronics/appkit/i18n/cs.h>
#include <felitronics/appkit/i18n/de.h>
#include <felitronics/appkit/i18n/es.h>
#include <felitronics/appkit/i18n/fr.h>
#include <felitronics/appkit/i18n/it.h>
#include <felitronics/appkit/i18n/pl.h>
#include <felitronics/appkit/i18n/pt.h>
#include <felitronics/appkit/i18n/ro.h>
#include <felitronics/appkit/i18n/ru.h>
#include <felitronics/appkit/i18n/tr.h>
#include <felitronics/appkit/i18n/uk.h>

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>
#include <utility>

namespace felitronics::appkit::i18n
{
struct Language
{
    std::string_view          code;
    std::span<const Entry>    entries;   // empty for English: the keys are its sentences
    std::span<const Counted>  counted;
};

// The site's order.
inline constexpr Language kLanguages[] {
    { "en", {},             tables::enCounted },
    { "de", tables::de,     tables::deCounted },
    { "uk", tables::uk,     tables::ukCounted },
    { "ru", tables::ru,     tables::ruCounted },
    { "cs", tables::cs,     tables::csCounted },
    { "es", tables::es,     tables::esCounted },
    { "fr", tables::fr,     tables::frCounted },
    { "it", tables::it,     tables::itCounted },
    { "pl", tables::pl,     tables::plCounted },
    { "pt", tables::pt,     tables::ptCounted },
    { "ro", tables::ro,     tables::roCounted },
    { "tr", tables::tr,     tables::trCounted },
};

inline const Language* find (std::string_view code) noexcept
{
    for (const auto& l : kLanguages)
        if (l.code == code)
            return &l;
    return nullptr;
}

/** One of the twelve codes for a locale name ("de", "de-AT", "pt_BR", "PT"), "en" for anything else. */
inline juce::String languageOf (const juce::String& locale)
{
    const auto code = locale.trim().upToFirstOccurrenceOf ("-", false, false)
                                   .upToFirstOccurrenceOf ("_", false, false).toLowerCase();
    return find (code.toRawUTF8()) != nullptr ? code : juce::String ("en");
}

namespace detail
{
inline juce::String& chosen() { static juce::String code; return code; }   // empty: the system's
inline const juce::String& systemCode()
{
    static const juce::String code = languageOf (juce::SystemStats::getDisplayLanguage());
    return code;
}
} // namespace detail

/** Overrides the system's language; an empty code goes back to it. An unknown code is English. */
inline void setLanguage (const juce::String& code)
{
    detail::chosen() = code.isEmpty() ? juce::String() : languageOf (code);
}

/** The language the windows speak now: one of the twelve codes. */
inline juce::String language()
{
    return detail::chosen().isNotEmpty() ? detail::chosen() : detail::systemCode();
}

/** CLDR's cardinal plural category of a whole number (its magnitude), for one of the twelve languages; the English
    rule for any other code. The rules are CLDR's for integers (no visible fraction digits, no compact exponent):
    https://www.unicode.org/cldr/charts/latest/supplemental/language_plural_rules.html */
inline Plural plural (std::string_view lang, std::int64_t value) noexcept
{
    const std::uint64_t n = value < 0 ? 0u - static_cast<std::uint64_t> (value) : static_cast<std::uint64_t> (value);
    const auto m10 = n % 10, m100 = n % 100;
    const bool million = n != 0 && n % 1000000 == 0;   // "many" in the Romance languages: 1 000 000 de días
    const bool few234  = m10 >= 2 && m10 <= 4 && ! (m100 >= 12 && m100 <= 14);

    if (lang == "fr" || lang == "pt") return n <= 1 ? Plural::one : million ? Plural::many : Plural::other;
    if (lang == "es" || lang == "it") return n == 1 ? Plural::one : million ? Plural::many : Plural::other;
    if (lang == "ro") return n == 1 ? Plural::one : (n == 0 || (m100 >= 1 && m100 <= 19)) ? Plural::few : Plural::other;
    if (lang == "cs") return n == 1 ? Plural::one : (n >= 2 && n <= 4) ? Plural::few : Plural::other;
    if (lang == "pl") return n == 1 ? Plural::one : few234 ? Plural::few : Plural::many;
    if (lang == "ru" || lang == "uk")
        return (m10 == 1 && m100 != 11) ? Plural::one : few234 ? Plural::few : Plural::many;
    return n == 1 ? Plural::one : Plural::other;   // en, de, tr
}

inline juce::String utf8 (std::string_view s) { return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size())); }

/** The current language's sentence for an English key; the key itself when the language has none. */
inline juce::String tr (const char* english)
{
    const std::string_view key (english != nullptr ? english : "");
    if (const auto* l = find (language().toRawUTF8()))
        for (const auto& e : l->entries)
            if (view (e.english) == key)
                return utf8 (view (e.text));
    return utf8 (key);
}

inline juce::String tr (const juce::String& english) { return tr (english.toRawUTF8()); }

/** Fills {name} placeholders in one pass: a value is inserted as it is, never scanned for placeholders itself. An
    unknown placeholder stays as written. */
inline juce::String fill (const juce::String& text, std::initializer_list<std::pair<std::string_view, juce::String>> values)
{
    const std::string_view s (text.toRawUTF8());
    std::string out;
    for (std::size_t i = 0; i < s.size();)
    {
        if (s[i] == '{')
            if (const auto close = s.find ('}', i); close != std::string_view::npos)
            {
                const auto name = s.substr (i + 1, close - i - 1);
                const juce::String* value = nullptr;
                for (const auto& v : values)
                    if (v.first == name)
                        value = &v.second;
                if (value != nullptr)
                {
                    out += value->toRawUTF8();
                    i = close + 1;
                    continue;
                }
            }
        out += s[i++];
    }
    return utf8 (out);
}

/** A counted phrase: the form the current language uses for `n`, with {n} filled in. The key is the English
    phrase's `other` form. Falls back to the English form for `n`, then to the key. */
inline juce::String trCount (const char* english, std::int64_t n)
{
    const std::string_view key (english != nullptr ? english : "");
    auto pick = [&] (const Language& l) -> const char8_t*
    {
        const auto form = plural (l.code, n);
        for (const auto& c : l.counted)
            if (view (c.english) == key && c.form == form)
                return c.text;
        return nullptr;
    };
    const auto* text = [&]() -> const char8_t*
    {
        if (const auto* l = find (language().toRawUTF8()))
            if (const auto* t = pick (*l))
                return t;
        return pick (kLanguages[0]);
    }();
    return fill (text != nullptr ? utf8 (view (text)) : utf8 (key), { { "n", juce::String (n) } });
}
} // namespace felitronics::appkit::i18n

// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// I18n.h: the tables are complete and say nothing English does not, keep every placeholder, are exactly the UTF-8
// they were written as (MSVC compiles them without /utf-8), and the plural rules give CLDR's own sample numbers the
// category CLDR gives them.

#include <juce_core/juce_core.h>
#include <felitronics/appkit/I18n.h>

#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace felitronics::appkit::i18n;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

struct Sample
{
    const char*            lang;
    Plural                 form;
    std::vector<long long> numbers;
};

// CLDR plurals.xml (unicode-org/cldr, common/supplemental), the @integer samples of each category: ranges
// expanded, the compact forms ("1c6") and the "…" dropped. Generated from the file, not typed.
static const Sample kCldrSamples[] {
    { "en", Plural::one, { 1 } },
    { "en", Plural::other, { 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 100, 1000, 10000, 100000, 1000000 } },
    { "de", Plural::one, { 1 } },
    { "de", Plural::other, { 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 100, 1000, 10000, 100000, 1000000 } },
    { "uk", Plural::one, { 1, 21, 31, 41, 51, 61, 71, 81, 101, 1001 } },
    { "uk", Plural::few, { 2, 3, 4, 22, 23, 24, 32, 33, 34, 42, 43, 44, 52, 53, 54, 62, 102, 1002 } },
    { "uk", Plural::many, { 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 100, 1000, 10000, 100000, 1000000 } },
    { "ru", Plural::one, { 1, 21, 31, 41, 51, 61, 71, 81, 101, 1001 } },
    { "ru", Plural::few, { 2, 3, 4, 22, 23, 24, 32, 33, 34, 42, 43, 44, 52, 53, 54, 62, 102, 1002 } },
    { "ru", Plural::many, { 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 100, 1000, 10000, 100000, 1000000 } },
    { "cs", Plural::one, { 1 } },
    { "cs", Plural::few, { 2, 3, 4 } },
    { "cs", Plural::other, { 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 100, 1000, 10000, 100000, 1000000 } },
    { "es", Plural::one, { 1 } },
    { "es", Plural::many, { 1000000 } },
    { "es", Plural::other, { 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 100, 1000, 10000, 100000 } },
    { "fr", Plural::one, { 0, 1 } },
    { "fr", Plural::many, { 1000000 } },
    { "fr", Plural::other, { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 100, 1000, 10000, 100000 } },
    { "it", Plural::one, { 1 } },
    { "it", Plural::many, { 1000000 } },
    { "it", Plural::other, { 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 100, 1000, 10000, 100000 } },
    { "pl", Plural::one, { 1 } },
    { "pl", Plural::few, { 2, 3, 4, 22, 23, 24, 32, 33, 34, 42, 43, 44, 52, 53, 54, 62, 102, 1002 } },
    { "pl", Plural::many, { 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 100, 1000, 10000, 100000, 1000000 } },
    { "pt", Plural::one, { 0, 1 } },
    { "pt", Plural::many, { 1000000 } },
    { "pt", Plural::other, { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 100, 1000, 10000, 100000 } },
    { "ro", Plural::one, { 1 } },
    { "ro", Plural::few, { 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 101, 1001 } },
    { "ro", Plural::other, { 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 100, 1000, 10000, 100000, 1000000 } },
    { "tr", Plural::one, { 1 } },
    { "tr", Plural::other, { 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 100, 1000, 10000, 100000, 1000000 } },
};

static const char* name (Plural p)
{
    switch (p)
    {
        case Plural::one:   return "one";
        case Plural::few:   return "few";
        case Plural::many:  return "many";
        case Plural::other: return "other";
    }
    return "?";
}

// The placeholders of a sentence, as a multiset: "{a} {b} {a}" -> { {a}, {a}, {b} }.
static std::multiset<std::string> placeholders (std::string_view s)
{
    std::multiset<std::string> out;
    for (std::size_t i = 0; (i = s.find ('{', i)) != std::string_view::npos;)
    {
        const auto close = s.find ('}', i);
        if (close == std::string_view::npos) break;
        out.insert (std::string (s.substr (i, close - i + 1)));
        i = close + 1;
    }
    return out;
}

// Well-formed UTF-8 (no overlong forms, no surrogates), decoded to code points; empty on a malformed byte.
static std::vector<char32_t> decode (std::string_view s, bool& good)
{
    std::vector<char32_t> cps;
    good = true;
    for (std::size_t i = 0; i < s.size();)
    {
        const auto b = static_cast<unsigned char> (s[i]);
        int n = b < 0x80 ? 0 : (b & 0xe0) == 0xc0 ? 1 : (b & 0xf0) == 0xe0 ? 2 : (b & 0xf8) == 0xf0 ? 3 : -1;
        if (n < 0 || i + static_cast<std::size_t> (n) >= s.size() + (n == 0 ? 1 : 0)) { good = false; return {}; }
        char32_t cp = n == 0 ? b : n == 1 ? (b & 0x1fu) : n == 2 ? (b & 0x0fu) : (b & 0x07u);
        for (int k = 1; k <= n; ++k)
        {
            const auto c = static_cast<unsigned char> (s[i + static_cast<std::size_t> (k)]);
            if ((c & 0xc0) != 0x80) { good = false; return {}; }
            cp = (cp << 6) | (c & 0x3fu);
        }
        const char32_t min = n == 1 ? 0x80 : n == 2 ? 0x800 : n == 3 ? 0x10000 : 0;
        if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) { good = false; return {}; }
        cps.push_back (cp);
        i += static_cast<std::size_t> (n) + 1;
    }
    return cps;
}

int main()
{
    std::printf ("felitronics::appkit i18n tests\n");
    const auto& english = tables::en;
    std::set<std::string> keys;
    for (const auto* k : english) keys.insert (std::string (view (k)));
    ok (keys.size() == std::size (english), "the English keys are distinct");

    const std::vector<std::string> codes { "en", "de", "uk", "ru", "cs", "es", "fr", "it", "pl", "pt", "ro", "tr" };
    group ("the twelve languages, in the site's order");
    {
        ok (std::size (kLanguages) == codes.size(), "twelve languages");
        for (std::size_t i = 0; i < codes.size() && i < std::size (kLanguages); ++i)
            ok (kLanguages[i].code == codes[i], "language " + std::to_string (i) + " is " + codes[i]);
    }

    // CLDR's integer categories per language: the categories that HAVE integer samples.
    std::map<std::string, std::set<Plural>> cldrForms;
    for (const auto& s : kCldrSamples) cldrForms[s.lang].insert (s.form);

    group ("every table translates every English key once, and nothing else");
    for (const auto& lang : kLanguages)
    {
        if (lang.code == "en") continue;
        const std::string code (lang.code);
        std::map<std::string, int> seen;
        for (const auto& e : lang.entries)
        {
            const std::string k (view (e.english)), v (view (e.text));
            ++seen[k];
            ok (keys.count (k) == 1, code + ": \"" + k + "\" is an English key");
            const auto jv = juce::String::fromUTF8 (v.c_str());
            ok (jv.trim().isNotEmpty() && jv.trim() == jv,
                code + ": the translation of \"" + k + "\" is not empty and has no stray whitespace");
            ok (placeholders (k) == placeholders (v), code + ": \"" + v + "\" keeps the placeholders of \"" + k + "\"");
            bool good = false;
            decode (v, good);
            ok (good, code + ": \"" + k + "\" is well-formed UTF-8");
        }
        int missing = 0, doubled = 0;
        for (const auto& k : keys)
        {
            if (seen[k] == 0) { ++missing; ok (false, code + " lacks \"" + k + "\""); }
            if (seen[k] > 1)  { ++doubled; ok (false, code + " has \"" + k + "\" " + std::to_string (seen[k]) + " times"); }
        }
        ok (missing == 0 && doubled == 0 && lang.entries.size() == keys.size(),
            code + ": " + std::to_string (lang.entries.size()) + " entries for " + std::to_string (keys.size()) + " keys");

        // Counted: exactly the forms CLDR's rules use for whole numbers in this language, each once, {n} kept.
        std::map<std::string, std::multiset<Plural>> forms;
        for (const auto& c : lang.counted)
        {
            forms[std::string (view (c.english))].insert (c.form);
            ok (placeholders (view (c.text)) == std::multiset<std::string> { "{n}" }, code + ": a counted form keeps {n} only");
            ok (! view (c.text).empty(), code + ": a counted form is not empty");
        }
        for (const auto& ec : tables::enCounted)
        {
            const std::string k (view (ec.english));
            const auto& f = forms[k];
            const std::set<Plural> distinct (f.begin(), f.end());
            ok (distinct == cldrForms[code] && f.size() == distinct.size(),
                code + ": \"" + k + "\" has one form for each of CLDR's integer categories");
        }
        ok (forms.size() == 1, code + ": no counted phrase English lacks");
    }

    group ("the bytes are the UTF-8 that was written (the tables are read by MSVC without /utf-8)");
    {
        // The escapes are the oracle: an ASCII spelling of the same sentence, which no source charset can change.
        auto is = [] (const char* lang, const char* key, const char* escaped)
        {
            setLanguage (lang);
            const auto got = tr (key);
            ok (got == juce::String::fromUTF8 (escaped) && std::string (got.toRawUTF8()) == escaped,
                std::string (lang) + ": \"" + key + "\" is exactly its UTF-8 bytes");
        };
        is ("ru", "Back", "\xd0\x9d\xd0\xb0\xd0\xb7\xd0\xb0\xd0\xb4");                              // Cyrillic
        is ("uk", "Back", "\xd0\x9d\xd0\xb0\xd0\xb7\xd0\xb0\xd0\xb4");                              // Cyrillic
        is ("uk", "Close", "\xd0\x97\xd0\xb0\xd0\xba\xd1\x80\xd0\xb8\xd1\x82\xd0\xb8");             // Cyrillic
        is ("uk", "Licence", "\xd0\x9b\xd1\x96\xd1\x86\xd0\xb5\xd0\xbd\xd0\xb7\xd1\x96\xd1\x8f");   // U+0456, the Ukrainian i
        is ("de", "Close", "Schlie\xc3\x9f" "en");                                                  // sharp s
        is ("ro", "Licence", "Licen\xc8\x9b\xc4\x83");                                              // t with a comma below, a breve
        is ("cs", "Back", "Zp\xc4\x9bt");                                                           // e with a caron
        is ("pl", "Download", "Pobierz");
        is ("pl", "Licence", "Licencja");
        is ("tr", "Back", "Geri");
        is ("tr", "Licence", "Lisans");
        is ("tr", "Download", "\xc4\xb0ndir");                                                      // capital I with a dot
        is ("fr", "Back", "Retour");
        is ("fr", "Download", "T\xc3\xa9l\xc3\xa9" "charger");                                      // two acute e
        is ("es", "Back", "Atr\xc3\xa1s");                                                          // acute a
        is ("it", "Back", "Indietro");
        is ("pt", "Licence", "Licen\xc3\xa7" "a");                                                  // c with a cedilla
        is ("en", "copied \xe2\x9c\x93", "copied \xe2\x9c\x93");                                    // the key itself
        setLanguage ({});

        // Romanian writes S and T with a comma below; the cedilla forms are another letter.
        bool cedilla = false;
        for (const auto& e : tables::ro)
            for (auto cp : [&] { bool g = false; return decode (view (e.text), g); }())
                cedilla = cedilla || cp == 0x15e || cp == 0x15f || cp == 0x162 || cp == 0x163;
        ok (! cedilla, "ro: no S or T with a cedilla");
    }

    group ("tr(): the current language's sentence, English when there is none");
    {
        setLanguage ("de");
        ok (language() == "de", "setLanguage (\"de\") is German");
        ok (tr ("Check for updates") == juce::String::fromUTF8 ("Nach Updates suchen"), "a German key");
        ok (tr (juce::String ("Check for updates")) == juce::String::fromUTF8 ("Nach Updates suchen"), "the same through a juce::String");
        ok (tr ("A sentence no table has") == "A sentence no table has", "an unknown sentence comes back as it is");
        ok (tr ("") == "", "an empty key is empty");
        ok (tr ("Feed the Cat") == "Feed the Cat", "the tip jar's name is not a key");
        setLanguage ("pt-BR");  ok (language() == "pt", "pt-BR is Portuguese");
        setLanguage ("PT_br");  ok (language() == "pt", "PT_br is Portuguese");
        setLanguage ("uk-UA");  ok (language() == "uk", "uk-UA is Ukrainian");
        setLanguage ("zh-Hans"); ok (language() == "en" && tr ("Back") == "Back", "a language not among the twelve is English");
        setLanguage ("xx");     ok (language() == "en", "an unknown code is English");
        setLanguage ({});
        ok (language() == languageOf (juce::SystemStats::getDisplayLanguage()), "no override: the system's display language");
        ok (languageOf (" de-AT ") == "de" && languageOf ("en_GB") == "en" && languageOf ("") == "en" && languageOf ("ro") == "ro",
            "languageOf takes the part before '-' or '_'");
        for (const auto& c : codes) ok (languageOf (c) == juce::String (c), "languageOf (\"" + c + "\") is itself");
    }

    group ("fill(): one pass, values never re-read");
    {
        ok (fill ("{a} and {b}", { { "a", "1" }, { "b", "2" } }) == "1 and 2", "two placeholders");
        ok (fill ("{a}{a}", { { "a", "x" } }) == "xx", "one placeholder twice");
        ok (fill ("{a}", { { "a", "{b}" }, { "b", "no" } }) == "{b}", "a value that looks like a placeholder stays a value");
        ok (fill ("{unknown} {", { { "a", "x" } }) == "{unknown} {", "an unknown placeholder and a lone brace stay");
        ok (fill (juce::String::fromUTF8 ("\xc2\xb7{n}\xc2\xb7"), { { "n", juce::String::fromUTF8 ("\xe2\x9c\x93") } })
                == juce::String::fromUTF8 ("\xc2\xb7\xe2\x9c\x93\xc2\xb7"), "non-ASCII on both sides of a placeholder");
    }

    group ("plural(): CLDR's integer samples get CLDR's category");
    {
        int n = 0;
        for (const auto& s : kCldrSamples)
            for (auto v : s.numbers)
            {
                ++n;
                const auto got = plural (s.lang, v);
                ok (got == s.form, std::string (s.lang) + " " + std::to_string (v) + " is " + name (s.form) + ", got " + name (got));
            }
        std::printf ("    %d CLDR samples\n", n);

        // ...and over every whole number to two million, no category CLDR does not list for integers.
        for (const auto& c : codes)
        {
            std::set<Plural> used;
            for (long long v = 0; v <= 2000000; ++v) used.insert (plural (c, v));
            ok (used == cldrForms[c], c + ": the rule uses exactly CLDR's integer categories up to 2 000 000");
            ok (plural (c, -21) == plural (c, 21) && plural (c, -1) == plural (c, 1), c + ": a negative count is its magnitude");
        }
    }

    group ("trCount(): the form for the number, {n} filled");
    {
        auto is = [] (const char* lang, long long v, const char* expected)
        {
            setLanguage (lang);
            const auto got = trCount ("Trial \xc2\xb7 {n} days left", v);
            ok (got == juce::String::fromUTF8 (expected), std::string (lang) + " " + std::to_string (v) + ": \""
                                                          + got.toStdString() + "\" is \"" + expected + "\"");
        };
        is ("en", 1, "Trial \xc2\xb7 1 day left");
        is ("en", 0, "Trial \xc2\xb7 0 days left");
        is ("en", 23, "Trial \xc2\xb7 23 days left");
        is ("de", 1, "Testphase \xc2\xb7 noch 1 Tag");
        is ("de", 30, "Testphase \xc2\xb7 noch 30 Tage");
        is ("ru", 1,  "\xd0\x9f\xd1\x80\xd0\xbe\xd0\xb1\xd0\xbd\xd1\x8b\xd0\xb9 \xd0\xbf\xd0\xb5\xd1\x80\xd0\xb8\xd0\xbe\xd0\xb4 \xc2\xb7 \xd0\xbe\xd1\x81\xd1\x82\xd0\xb0\xd0\xbb\xd1\x81\xd1\x8f 1 \xd0\xb4\xd0\xb5\xd0\xbd\xd1\x8c");
        is ("ru", 21, "\xd0\x9f\xd1\x80\xd0\xbe\xd0\xb1\xd0\xbd\xd1\x8b\xd0\xb9 \xd0\xbf\xd0\xb5\xd1\x80\xd0\xb8\xd0\xbe\xd0\xb4 \xc2\xb7 \xd0\xbe\xd1\x81\xd1\x82\xd0\xb0\xd0\xbb\xd1\x81\xd1\x8f 21 \xd0\xb4\xd0\xb5\xd0\xbd\xd1\x8c");
        is ("ru", 22, "\xd0\x9f\xd1\x80\xd0\xbe\xd0\xb1\xd0\xbd\xd1\x8b\xd0\xb9 \xd0\xbf\xd0\xb5\xd1\x80\xd0\xb8\xd0\xbe\xd0\xb4 \xc2\xb7 \xd0\xbe\xd1\x81\xd1\x82\xd0\xb0\xd0\xbb\xd0\xbe\xd1\x81\xd1\x8c 22 \xd0\xb4\xd0\xbd\xd1\x8f");
        is ("ru", 11, "\xd0\x9f\xd1\x80\xd0\xbe\xd0\xb1\xd0\xbd\xd1\x8b\xd0\xb9 \xd0\xbf\xd0\xb5\xd1\x80\xd0\xb8\xd0\xbe\xd0\xb4 \xc2\xb7 \xd0\xbe\xd1\x81\xd1\x82\xd0\xb0\xd0\xbb\xd0\xbe\xd1\x81\xd1\x8c 11 \xd0\xb4\xd0\xbd\xd0\xb5\xd0\xb9");
        is ("pl", 5,  "Wersja pr\xc3\xb3" "bna \xc2\xb7 zosta\xc5\x82" "o 5 dni");
        is ("pl", 22, "Wersja pr\xc3\xb3" "bna \xc2\xb7 zosta\xc5\x82y 22 dni");
        is ("cs", 3,  "Zku\xc5\xa1" "ebn\xc3\xad verze \xc2\xb7 zb\xc3\xbdvaj\xc3\xad 3 dny");
        is ("ro", 20, "Prob\xc4\x83 \xc2\xb7 au mai r\xc4\x83mas 20 de zile");
        is ("fr", 0,  "Essai \xc2\xb7 0 jour restant");
        is ("es", 1000000, "Prueba \xc2\xb7 quedan 1000000 de d\xc3\xad" "as");
        setLanguage ("xx");
        ok (trCount ("Trial \xc2\xb7 {n} days left", 2) == juce::String::fromUTF8 ("Trial \xc2\xb7 2 days left"), "no table: English");
        ok (trCount ("No such phrase {n}", 2) == "No such phrase 2", "an unknown counted phrase: the key, filled");
        setLanguage ({});
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

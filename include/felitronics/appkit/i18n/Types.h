// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

// i18n/Types.h — the shape of a translation table (see I18n.h). Plain C++, no JUCE.
//
// The tables are UTF-8 string literals written as u8"..." in files that start with a UTF-8 byte order mark. Both are
// for MSVC: without /utf-8 it reads a file with no mark in the machine's code page, and turns a plain "..." into that
// code page as well, so Cyrillic would come out as question marks. The mark fixes the reading, u8 fixes the writing;
// gcc and clang take both as they are.

#include <cstddef>
#include <string_view>

namespace felitronics::appkit::i18n
{
// CLDR's cardinal plural categories. Only the four the twelve languages use for whole numbers.
enum class Plural { one, few, many, other };

// One sentence: the English key (exactly the text the code passes to tr()) and its translation.
struct Entry
{
    const char8_t* english;
    const char8_t* text;
};

// One form of a counted phrase: the key is the English phrase's `other` form, whatever form this entry is.
struct Counted
{
    const char8_t* english;
    Plural         form;
    const char8_t* text;
};

inline std::string_view view (const char8_t* s) noexcept
{
    return s != nullptr ? std::string_view (reinterpret_cast<const char*> (s)) : std::string_view();
}
} // namespace felitronics::appkit::i18n

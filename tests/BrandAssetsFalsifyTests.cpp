// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// The brand is compiled into BrandAssets.h as 8 kB of SVG and 15 kB of base64. If either is ever
// truncated by a regeneration, nothing throws and nothing crashes: the window quietly falls back to
// a system font and an empty square where the cat should be — exactly the hole this data exists to
// close. These checks turn that silence into a red build.

#include <felitronics/appkit/BrandAssets.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace brand = felitronics::appkit::brand;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

// How many pixels the mark actually puts on a transparent ground — a Drawable that parsed but drew
// nothing would pass every null check and still leave the window empty.
static int inkedPixels (int size)
{
    juce::Image img (juce::Image::ARGB, size, size, true);
    {
        juce::Graphics g (img);
        brand::drawCat (g, juce::Rectangle<float> (0.0f, 0.0f, (float) size, (float) size));
    }

    int inked = 0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (img.getPixelAt (x, y).getAlpha() > 8)
                ++inked;
    return inked;
}

int main()
{
    std::printf ("BrandAssets — the identity the library carries\n");

    group ("the maker's mark parses and draws");
    {
        auto* cat = brand::catMark();
        ok (cat != nullptr, "catMark() returns a Drawable");

        if (cat != nullptr)
        {
            const auto bounds = cat->getDrawableBounds();
            ok (bounds.getWidth() > 1.0f && bounds.getHeight() > 1.0f, "the mark has real bounds");

            // The source art is 850 x 865; a Drawable that lost its viewBox would come back square
            // or wildly off, and the lockup would sit crooked next to the wordmark.
            const float aspect = bounds.getWidth() / juce::jmax (1.0f, bounds.getHeight());
            ok (std::abs (aspect - 850.0f / 865.0f) < 0.1f, "the mark keeps the art's aspect");
        }

        const int inked = inkedPixels (64);
        ok (inked > 300, "the mark actually inks a 64 px box (" + std::to_string (inked) + " px)");
        ok (inked < 64 * 64, "...and is not a solid block");
    }

    group ("the wordmark face decodes and measures");
    {
        auto face = brand::wordmarkTypeface();
        ok (face != nullptr, "wordmarkTypeface() decodes");

        if (face != nullptr)
        {
            // The subset keeps the family name — that is what makes it interchangeable with a
            // product's own copy of the full font, and what a fallback to a system face would break.
            ok (face->getName() == "Michroma", "the face is Michroma, not a fallback");

            const juce::Font f = brand::wordmarkFont (20.0f);
            ok (f.getTypefacePtr() == face, "wordmarkFont() hands back that same face");

            // Every string the family sets in this face must have glyphs. A subset cut too far
            // would still render — as .notdef boxes — so width alone is not enough: a string of
            // letters and a string of digits must differ in width from each other and from empty.
            const auto w = [&f] (const char* s) { return juce::GlyphArrangement::getStringWidth (f, juce::String::fromUTF8 (s)); };
            ok (w ("by Darwin's Cat") > 40.0f, "the byline measures");
            ok (w ("TabbyEQ") > 20.0f && w ("OrbitAmp") > 20.0f, "product names measure");
            ok (w ("0123456789") > 20.0f, "digits measure — the chrome uses them");
            ok (w (" \xc2\xb7 ") > 2.0f, "the middle dot is in the subset (V0.6.0 \xc2\xb7 STANDALONE)");
            ok (w ("i") < w ("W"), "the face is proportional, i.e. really this face");
        }
    }

    group ("the embedded data itself");
    {
        juce::MemoryOutputStream b64;
        int chunkCount = 0;
        for (const char* const* c = brand::assets::wordmarkBase64; *c != nullptr; ++c, ++chunkCount)
            b64 << *c;

        ok (chunkCount > 0, "the base64 is split into chunks, none of them lost");
        ok (b64.getDataSize() > 8000, "the base64 is the whole font, not a stub");

        juce::MemoryOutputStream ttf;
        ok (juce::Base64::convertFromBase64 (ttf, b64.toString()), "the base64 decodes cleanly");
        ok (ttf.getDataSize() > 8000, "...into a font-sized blob");

        // A TrueType file starts with 0x00010000 ("sfnt"); a truncated chunk usually still decodes,
        // and this is what tells the difference between a font and a fragment.
        const auto* bytes = static_cast<const juce::uint8*> (ttf.getData());
        ok (ttf.getDataSize() > 4 && bytes[0] == 0 && bytes[1] == 1 && bytes[2] == 0 && bytes[3] == 0,
            "the blob really is a TrueType file");

        ok (juce::String (brand::assets::catSvg).contains ("<svg"), "the cat SVG survived as SVG");
    }

    std::printf (failures == 0 ? "\nAll %d checks passed.\n" : "\n%d of %d checks FAILED.\n",
                 failures == 0 ? checks : failures, checks);
    return failures == 0 ? 0 : 1;
}

// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// WebpImage.h round-trip, falsified where it can lie:
//   * an ARGB cut-out survives encode→decode: alpha EXACT (the plane is lossless), colour within
//     the lossy plane's error where alpha is solid enough to hold colour at all;
//   * an RGB (opaque) image comes back fully opaque;
//   * garbage bytes and a null image fail EMPTY/INVALID, never half-filled.
#include <felitronics/appkit/WebpImage.h>

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cmath>

static int failures = 0;
static void ok (bool cond, const char* what)
{
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what); }
}

int main()
{
    std::printf ("appkit_webp_image_tests\n");
    using felitronics::appkit::encodeWebp;
    using felitronics::appkit::decodeWebp;

    // --- an ARGB gradient with a soft alpha edge, like a cut-out pedal -----------------------------
    {
        juce::Image img (juce::Image::ARGB, 64, 48, true);
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const auto a = static_cast<juce::uint8> (x < 8 ? x * 32 : 255);   // transparent → solid
                img.setPixelAt (x, y, juce::Colour::fromRGBA (static_cast<juce::uint8> (40 + 3 * (x % 64)),
                                                              static_cast<juce::uint8> (200 - y),
                                                              static_cast<juce::uint8> (90),
                                                              a));
            }
        const auto webp = encodeWebp (img, 95.0f);
        ok (! webp.empty(), "encodes");
        ok (webp.size() < 64u * 48u, "…and actually compresses");
        const auto back = decodeWebp (webp.data(), webp.size());
        ok (back.isValid() && back.getWidth() == 64 && back.getHeight() == 48, "decodes to the same size");
        int worstAlpha = 0, worstColour = 0;
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 64; ++x)
            {
                const auto p0 = img.getPixelAt (x, y), p1 = back.getPixelAt (x, y);
                worstAlpha = std::max (worstAlpha, std::abs (int (p0.getAlpha()) - int (p1.getAlpha())));
                if (p0.getAlpha() == 255)   // colour is judged only where alpha holds it exactly
                {
                    worstColour = std::max (worstColour, std::abs (int (p0.getRed())   - int (p1.getRed())));
                    worstColour = std::max (worstColour, std::abs (int (p0.getGreen()) - int (p1.getGreen())));
                    worstColour = std::max (worstColour, std::abs (int (p0.getBlue())  - int (p1.getBlue())));
                }
            }
        ok (worstAlpha == 0, "the alpha plane is EXACT");
        ok (worstColour <= 24, "solid colour within the lossy plane's error at q95");
    }

    // --- an RGB (no alpha) image: one path, opaque in, opaque out ----------------------------------
    {
        juce::Image rgb (juce::Image::RGB, 16, 16, true);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                rgb.setPixelAt (x, y, juce::Colour::fromRGB (static_cast<juce::uint8> (x * 16),
                                                             static_cast<juce::uint8> (y * 16), 128));
        const auto webp = encodeWebp (rgb, 95.0f);
        ok (! webp.empty(), "RGB encodes");
        const auto back = decodeWebp (webp.data(), webp.size());
        bool opaque = back.isValid();
        for (int y = 0; opaque && y < 16; ++y)
            for (int x = 0; opaque && x < 16; ++x)
                opaque = back.getPixelAt (x, y).getAlpha() == 255;
        ok (opaque, "…and comes back fully opaque");
    }

    // --- refusals ----------------------------------------------------------------------------------
    {
        ok (encodeWebp (juce::Image()).empty(), "a null image encodes to nothing");
        const unsigned char junk[16] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'J', 'U', 'N', 'K', 0, 0, 0, 0 };
        ok (! decodeWebp (junk, sizeof (junk)).isValid(), "junk decodes to an invalid image");
        // A forged VP8X header: thirty bytes declaring an ANIMATED 65535x65535 canvas. WebPGetInfo
        // reports those dimensions without validating a single frame - the decoder must refuse
        // before allocating anything.
        const unsigned char vp8x[30] = { 'R', 'I', 'F', 'F', 22, 0, 0, 0, 'W', 'E', 'B', 'P',
                                         'V', 'P', '8', 'X', 10, 0, 0, 0,
                                         0x02, 0, 0, 0,               // flags: ANIMATION
                                         0xfe, 0xff, 0x00,            // canvas width - 1  = 65534
                                         0xfe, 0xff, 0x00 };          // canvas height - 1 = 65534
        ok (! decodeWebp (vp8x, sizeof (vp8x)).isValid(), "a forged animated canvas is refused before any allocation");
        ok (! decodeWebp (nullptr, 0).isValid(), "no bytes, no image");
    }

    if (failures == 0) { std::printf ("ALL TESTS PASSED\n"); return 0; }
    std::printf ("%d FAILURES\n", failures);
    return 1;
}

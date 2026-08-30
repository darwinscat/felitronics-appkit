// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once
// felitronics::appkit — juce::Image ↔ WebP, for the one picture a device pack ships.
//
// WHY WEBP: the picture is a cut-out (alpha kept), so JPEG is out; PNG at pack sizes is megabytes
// where WebP is a hundred kilobytes; AVIF's decoders are a dependency swamp. Lossy WebP compresses
// the colour plane and keeps the ALPHA PLANE LOSSLESS by default, so the cut edge stays crisp.
//
// JUCE keeps ARGB images PREMULTIPLIED; WebP speaks straight (non-premultiplied) RGBA. Both
// directions convert honestly — encode un-premultiplies, decode premultiplies — so what a pixel
// loses on a round trip is the lossy colour plane's error and nothing structural.
//
// Consume by linking `felitronics::appkit_webp` (CMake option FELITRONICS_APPKIT_WEBP=ON — it
// fetches libwebp, pinned); the consumer supplies juce_graphics as with every JUCE header here.

#include <juce_graphics/juce_graphics.h>

#include <webp/decode.h>
#include <webp/encode.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace felitronics::appkit {

/// A juce::Image (any format) → WebP bytes: the colour plane lossy at `quality` (0..100), the alpha
/// plane lossless (libwebp's default). Empty on failure — a null image, a dimension WebP cannot
/// hold, or an encoder refusal.
inline std::vector<std::byte> encodeWebp (const juce::Image& img, float quality = 85.0f)
{
    if (! img.isValid()) return {};
    const int w = img.getWidth(), h = img.getHeight();
    if (w <= 0 || h <= 0 || w > WEBP_MAX_DIMENSION || h > WEBP_MAX_DIMENSION) return {};

    // One path for every source format: through ARGB. An RGB source becomes opaque alpha, which the
    // lossless alpha plane compresses to almost nothing.
    const juce::Image argb = img.getFormat() == juce::Image::ARGB ? img
                                                                  : img.convertedToFormat (juce::Image::ARGB);
    const juce::Image::BitmapData bd (argb, juce::Image::BitmapData::readOnly);
    const std::size_t rowBytes = static_cast<std::size_t> (w) * 4u;
    std::vector<std::uint8_t> straight (rowBytes * static_cast<std::size_t> (h));
    for (int y = 0; y < h; ++y)
    {
        const std::uint8_t* line = bd.getLinePointer (y);
        std::uint8_t* out = straight.data() + static_cast<std::size_t> (y) * rowBytes;
        for (int x = 0; x < w; ++x)
        {
            juce::PixelARGB px = *reinterpret_cast<const juce::PixelARGB*> (
                line + static_cast<std::size_t> (x) * static_cast<std::size_t> (bd.pixelStride));
            px.unpremultiply();                            // WebP wants straight alpha
            std::uint8_t* o = out + static_cast<std::size_t> (x) * 4u;
            o[0] = px.getBlue(); o[1] = px.getGreen(); o[2] = px.getRed(); o[3] = px.getAlpha();
        }
    }

    // libwebp VALIDATES quality rather than clamping it - out of range would come back as a bare
    // "encoder refused", indistinguishable from a real failure. Clamped here; a NaN falls to the
    // default rather than through jlimit's comparisons.
    const float q = std::isfinite (quality) ? juce::jlimit (0.0f, 100.0f, quality) : 85.0f;
    std::uint8_t* encoded = nullptr;
    const std::size_t n = WebPEncodeBGRA (straight.data(), w, h, static_cast<int> (rowBytes),
                                          q, &encoded);
    // Owned BEFORE the copy: if the vector's allocation throws, the encoder's buffer must not leak.
    const std::unique_ptr<std::uint8_t, decltype (&WebPFree)> owned (encoded, &WebPFree);
    if (n == 0 || encoded == nullptr) return {};
    return { reinterpret_cast<const std::byte*> (encoded),
             reinterpret_cast<const std::byte*> (encoded) + n };
}

/// WebP bytes → an ARGB juce::Image (premultiplied, as JUCE keeps them). Invalid on failure.
inline juce::Image decodeWebp (const void* bytes, std::size_t size)
{
    if (bytes == nullptr || size == 0) return {};
    const auto* data = static_cast<const std::uint8_t*> (bytes);
    // FEATURES BEFORE A SINGLE BYTE IS ALLOCATED. WebPGetInfo hands over a VP8X header's 24-bit
    // canvas before any frame is validated - a forged thirty-byte "animation" declaring 65535x65535
    // would otherwise walk a ~17 GB allocation straight into std::vector. Animation is refused (this
    // decodes ONE picture), and both sides are held to WebP's own hard ceiling.
    WebPBitstreamFeatures f;
    if (WebPGetFeatures (data, size, &f) != VP8_STATUS_OK) return {};
    if (f.has_animation != 0
        || f.width <= 0 || f.height <= 0
        || f.width > WEBP_MAX_DIMENSION || f.height > WEBP_MAX_DIMENSION) return {};
    const int w = f.width, h = f.height;

    const std::size_t rowBytes = static_cast<std::size_t> (w) * 4u;
    std::vector<std::uint8_t> straight (rowBytes * static_cast<std::size_t> (h));
    if (WebPDecodeBGRAInto (data, size, straight.data(), straight.size(), static_cast<int> (rowBytes)) == nullptr)
        return {};

    juce::Image img (juce::Image::ARGB, w, h, false);
    const juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
    {
        const std::uint8_t* in = straight.data() + static_cast<std::size_t> (y) * rowBytes;
        std::uint8_t* line = bd.getLinePointer (y);
        for (int x = 0; x < w; ++x)
        {
            const std::uint8_t* i = in + static_cast<std::size_t> (x) * 4u;
            juce::PixelARGB px;
            px.setARGB (i[3], i[2], i[1], i[0]);
            px.premultiply();                              // …and back into JUCE's convention
            *reinterpret_cast<juce::PixelARGB*> (
                line + static_cast<std::size_t> (x) * static_cast<std::size_t> (bd.pixelStride)) = px;
        }
    }
    return img;
}

} // namespace felitronics::appkit

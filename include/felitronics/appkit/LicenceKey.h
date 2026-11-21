// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

// LicenceKey.h — a paid product's key: what is in it, how it is written, how a product checks it, and which versions
// it covers. JUCE-free. Link felitronics::appkit_licence (it compiles Monocypher's Ed25519).
//
// A key is a few fields signed with Ed25519 (RFC 8032) and written as one base32 block. The product holds only the
// public key and checks the signature itself: no activation, no internet, no tie to a computer. The fields cannot be
// read from the block by eye, and changing any character of it makes the key "damaged"; nobody can make a key without
// the private key, which lives only where keys are issued. The exact bytes, for whoever issues keys:
// docs/licence-key.md.
//
// Updates: a key covers every version RELEASED on or before its `updatesUntil` day (a year after the purchase, or
// forever), whatever today's date is — a version once covered stays covered.
//
// Days are whole days since 1970-01-01 (UTC), as 16 bits: enough until 2149. kForever (0xffff) covers every version.
//
// The plain types (Key, Verdict, Status, the day arithmetic) are in LicenceTypes.h, which needs neither JUCE nor
// Monocypher.

#include <felitronics/appkit/LicenceTypes.h>

#include <monocypher-ed25519.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace felitronics::appkit::licence
{
using Bytes     = std::vector<std::uint8_t>;
using PublicKey = std::array<std::uint8_t, 32>;
using Seed      = std::array<std::uint8_t, 32>;   // the private half: only where keys are issued, never in a product

namespace detail
{
inline constexpr std::array<std::uint8_t, 3> kMagic { 'D', 'C', 'K' };
inline constexpr std::uint8_t kFormat = 1;
inline constexpr std::size_t  kMaxProduct = 32, kMaxName = 120, kMaxEmail = 120, kSignature = 64;
inline constexpr std::size_t  kFixed = 3 + 1 + 1 + 4 + 2 + 2 + 3;   // magic, format, kind, number, two days, 3 lengths
inline constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

inline bool productIdOk (std::string_view p) noexcept
{
    if (p.empty() || p.size() > kMaxProduct) return false;
    for (char c : p)
        if (! ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}

// Well-formed UTF-8 only: no overlong forms, no surrogates, nothing past U+10FFFF.
inline bool utf8Ok (std::string_view s) noexcept
{
    std::size_t i = 0;
    while (i < s.size())
    {
        const auto b = static_cast<std::uint8_t> (s[i]);
        std::size_t n = 0; std::uint32_t cp = 0, min = 0;
        if (b < 0x80) { ++i; continue; }
        if      ((b & 0xe0) == 0xc0) { n = 1; cp = b & 0x1fu; min = 0x80; }
        else if ((b & 0xf0) == 0xe0) { n = 2; cp = b & 0x0fu; min = 0x800; }
        else if ((b & 0xf8) == 0xf0) { n = 3; cp = b & 0x07u; min = 0x10000; }
        else return false;
        if (i + n >= s.size()) return false;   // the sequence runs past the end
        for (std::size_t k = 1; k <= n; ++k)
        {
            const auto c = static_cast<std::uint8_t> (s[i + k]);
            if ((c & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (c & 0x3fu);
        }
        if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
        i += n + 1;
    }
    return true;
}

inline void put16 (Bytes& b, std::uint16_t v) { b.push_back (static_cast<std::uint8_t> (v >> 8)); b.push_back (static_cast<std::uint8_t> (v & 0xffu)); }
inline void put32 (Bytes& b, std::uint32_t v) { put16 (b, static_cast<std::uint16_t> (v >> 16)); put16 (b, static_cast<std::uint16_t> (v & 0xffffu)); }
inline void putText (Bytes& b, std::string_view s) { b.push_back (static_cast<std::uint8_t> (s.size())); b.insert (b.end(), s.begin(), s.end()); }

inline std::string base32 (const Bytes& in)
{
    std::string out;
    std::uint32_t acc = 0; int bits = 0;
    for (auto byte : in)
    {
        acc = ((acc << 8) | byte) & 0xffffu; bits += 8;
        while (bits >= 5) { bits -= 5; out += kAlphabet[(acc >> bits) & 0x1fu]; }
    }
    if (bits > 0) out += kAlphabet[(acc << (5 - bits)) & 0x1fu];
    return out;
}

// Strict: only the alphabet, a length a byte string can have, and the unused bits of the last character zero — so no
// two texts read as the same key, and changing any one character changes the bytes.
inline std::optional<Bytes> unbase32 (std::string_view in)
{
    const auto r = in.size() % 8;
    if (r == 1 || r == 3 || r == 6) return std::nullopt;
    Bytes out; out.reserve (in.size() * 5 / 8);
    std::uint32_t acc = 0; int bits = 0;
    for (char c : in)
    {
        std::uint32_t v = 0;
        if (c >= 'A' && c <= 'Z') v = static_cast<std::uint32_t> (c - 'A');
        else if (c >= '2' && c <= '7') v = static_cast<std::uint32_t> (c - '2') + 26u;
        else return std::nullopt;
        acc = ((acc << 5) | v) & 0xffffu; bits += 5;
        if (bits >= 8) { bits -= 8; out.push_back (static_cast<std::uint8_t> ((acc >> bits) & 0xffu)); }
    }
    if (bits > 0 && (acc & ((1u << bits) - 1u)) != 0) return std::nullopt;
    return out;
}

// A paste, made plain: CRLF and a lone CR become LF, and the spaces a mail client or a word processor puts in (tab,
// no-break, the typographic and zero-width spaces, the byte order mark, the ideographic space) become ' '; the line
// and paragraph separators (U+2028/9, NEL) become LF.
inline std::string plain (std::string_view text)
{
    std::string out;
    out.reserve (text.size());
    for (std::size_t i = 0; i < text.size();)
    {
        const auto at = [&] (std::size_t k) { return i + k < text.size() ? static_cast<std::uint8_t> (text[i + k]) : std::uint8_t { 0 }; };
        const auto b0 = at (0), b1 = at (1), b2 = at (2);
        if (b0 == '\r')                                           { out += '\n'; i += b1 == '\n' ? 2 : 1; }
        else if (b0 == '\t' || b0 == '\v' || b0 == '\f')         { out += ' '; i += 1; }
        else if (b0 == 0xc2 && (b1 == 0xa0 || b1 == 0x85))       { out += b1 == 0x85 ? '\n' : ' '; i += 2; }   // NBSP, NEL
        else if (b0 == 0xe1 && b1 == 0x9a && b2 == 0x80)         { out += ' '; i += 3; }                        // U+1680
        else if (b0 == 0xe2 && b1 == 0x80 && (b2 <= 0x8b || b2 == 0xaf)) { out += ' '; i += 3; }                // U+2000..200B, U+202F
        else if (b0 == 0xe2 && b1 == 0x80 && (b2 == 0xa8 || b2 == 0xa9)) { out += '\n'; i += 3; }               // U+2028, U+2029
        else if (b0 == 0xe2 && b1 == 0x81 && (b2 == 0x9f || b2 == 0xa0)) { out += ' '; i += 3; }                // U+205F, U+2060
        else if (b0 == 0xe3 && b1 == 0x80 && b2 == 0x80)         { out += ' '; i += 3; }                        // U+3000
        else if (b0 == 0xef && b1 == 0xbb && b2 == 0xbf)         { out += ' '; i += 3; }                        // U+FEFF
        else                                                     { out += static_cast<char> (b0); i += 1; }
    }
    return out;
}

inline char upper (char c) noexcept { return (c >= 'a' && c <= 'z') ? static_cast<char> (c - 'a' + 'A') : c; }

// A key's marker: "-----BEGIN <NAME> KEY-----" or "-----END <NAME> KEY-----", any case, anywhere in the text (a block
// a mail client flattened to one line still has both). NAME is what a header carries: letters, digits, '-'. Nothing
// else is a key's marker: "-----BEGIN PGP SIGNED MESSAGE-----" is not.
struct Marker
{
    std::size_t start = 0, end = 0;   // where the marker's text is
    std::string name;                 // upper case
};

inline std::vector<Marker> markers (const std::string& s, std::string_view word)
{
    std::vector<Marker> out;
    const std::string open = "-----" + std::string (word) + " ";
    constexpr std::string_view close = " KEY-----";
    for (std::size_t i = 0; i + open.size() <= s.size(); ++i)
    {
        bool hit = true;
        for (std::size_t k = 0; k < open.size() && hit; ++k)
            hit = upper (s[i + k]) == open[k];
        if (! hit)
            continue;
        Marker m;
        m.start = i;
        std::size_t j = i + open.size();
        while (j < s.size() && m.name.size() <= kMaxProduct
               && ((s[j] >= 'A' && s[j] <= 'Z') || (s[j] >= 'a' && s[j] <= 'z') || (s[j] >= '0' && s[j] <= '9') || s[j] == '-'))
            m.name += upper (s[j++]);
        bool closed = ! m.name.empty() && j + close.size() <= s.size();
        for (std::size_t k = 0; k < close.size() && closed; ++k)
            closed = upper (s[j + k]) == close[k];
        if (! closed)
            continue;
        m.end = j + close.size();
        i = m.end - 1;
        out.push_back (std::move (m));
    }
    return out;
}

// The base32 characters a person pasted. When the paste holds key blocks, the block whose header names `product`
// (the first block when none does), from its BEGIN marker to the next END marker; anything around it is the letter's.
// Without a BEGIN marker, the whole paste. Inside, every line loses a mail client's quote marks ('>' or '|') in front;
// spaces and line breaks are dropped and lower case is raised; any other character makes the paste unreadable, and so
// does a BEGIN with no END after it.
inline std::optional<std::string> extract (std::string_view pasted, std::string_view product = {})
{
    const auto text = plain (pasted);
    std::string_view body (text);

    const auto begins = markers (text, "BEGIN");
    if (! begins.empty())
    {
        std::string wanted;
        for (char c : product) wanted += upper (c);
        const Marker* begin = &begins.front();
        for (const auto& b : begins)
            if (b.name == wanted) { begin = &b; break; }
        const Marker* end = nullptr;
        const auto ends = markers (text, "END");
        for (const auto& e : ends)
            if (e.start >= begin->end) { end = &e; break; }
        if (end == nullptr)
            return std::nullopt;   // a BEGIN without its END: cut short
        body = std::string_view (text).substr (begin->end, end->start - begin->end);
    }

    std::string out;
    bool lineStart = true;
    for (char c : body)
    {
        if (c == '\n')                                { lineStart = true; continue; }
        if (c == ' ')                                 continue;
        if (lineStart && (c == '>' || c == '|'))      continue;   // quote marks, before the line's first character
        lineStart = false;
        c = upper (c);
        if ((c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7')) out += c;
        else return std::nullopt;
    }
    if (out.empty()) return std::nullopt;
    return out;
}

// Public keys that let a forgery through. Ed25519 as RFC 8032 checks it does not refuse a public key of small order,
// and against one, a "signature" that is the identity point and a zero scalar checks for a share of all messages. A
// product's own key is never one of these; a damaged or zeroed one in a build could be, so the check refuses them. The
// list is libsodium's small-order blacklist (the sign bit of the last byte ignored, as libsodium does).
inline bool weakPublicKey (const PublicKey& pk) noexcept
{
    static constexpr std::uint8_t kSmallOrder[7][32] {
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0, order 4
        { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 1, order 1
        { 0x26, 0xe8, 0x95, 0x8f, 0xc2, 0xb2, 0x27, 0xb0, 0x45, 0xc3, 0xf4, 0x89, 0xf2, 0xef, 0x98, 0xf0,
          0xd5, 0xdf, 0xac, 0x05, 0xd3, 0xc6, 0x33, 0x39, 0xb1, 0x38, 0x02, 0x88, 0x6d, 0x53, 0xfc, 0x05 },   // order 8
        { 0xc7, 0x17, 0x6a, 0x70, 0x3d, 0x4d, 0xd8, 0x4f, 0xba, 0x3c, 0x0b, 0x76, 0x0d, 0x10, 0x67, 0x0f,
          0x2a, 0x20, 0x53, 0xfa, 0x2c, 0x39, 0xcc, 0xc6, 0x4e, 0xc7, 0xfd, 0x77, 0x92, 0xac, 0x03, 0x7a },   // order 8
        { 0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
          0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f },   // p-1, order 2
        { 0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
          0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f },   // p (= 0)
        { 0xee, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
          0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f },   // p+1 (= 1)
    };
    for (const auto& bad : kSmallOrder)
    {
        unsigned diff = static_cast<unsigned> (pk[31] & 0x7fu) ^ bad[31];
        for (std::size_t j = 0; j < 31; ++j)
            diff |= static_cast<unsigned> (pk[j] ^ bad[j]);
        if (diff == 0)
            return true;
    }
    return false;
}
} // namespace detail

/** The signed part of a key, exactly as docs/licence-key.md lays it out; nullopt when a field breaks its limits. */
inline std::optional<Bytes> payload (const Key& k)
{
    if (! detail::productIdOk (k.product) || k.name.size() > detail::kMaxName || k.email.size() > detail::kMaxEmail
        || ! detail::utf8Ok (k.name) || ! detail::utf8Ok (k.email))
        return std::nullopt;
    Bytes b (detail::kMagic.begin(), detail::kMagic.end());
    b.push_back (detail::kFormat);
    b.push_back (k.kind);
    detail::put32 (b, k.number);
    detail::put16 (b, k.issued);
    detail::put16 (b, k.updatesUntil);
    detail::putText (b, k.product);
    detail::putText (b, k.name);
    detail::putText (b, k.email);
    return b;
}

/** The block a person pastes: BEGIN line, base32 in lines of 40, END line. */
inline std::string text (const Bytes& signedKey, std::string_view product)
{
    std::string head;
    for (char c : product) head += (c >= 'a' && c <= 'z') ? static_cast<char> (c - 'a' + 'A') : c;
    const auto b32 = detail::base32 (signedKey);
    std::string out = "-----BEGIN " + head + " KEY-----\n";
    for (std::size_t i = 0; i < b32.size(); i += 40) out += b32.substr (i, 40) + "\n";
    return out + "-----END " + head + " KEY-----\n";
}

/** Reads a pasted key for `product`, checking its signature against `publicKey`. */
inline Reading read (std::string_view pasted, const PublicKey& publicKey, std::string_view product)
{
    Reading r;
    if (detail::weakPublicKey (publicKey)) return r;   // nothing checks against a key that lets forgeries through
    const auto chars = detail::extract (pasted, product);
    if (! chars) return r;
    const auto bytes = detail::unbase32 (*chars);
    if (! bytes || bytes->size() < detail::kFixed + 1 + detail::kSignature) return r;

    const auto signedSize = bytes->size() - detail::kSignature;
    if (crypto_ed25519_check (bytes->data() + signedSize, publicKey.data(), bytes->data(), signedSize) != 0) return r;

    // Signed by us: from here the bytes are ours, but still read with every bound checked.
    const auto* p = bytes->data();
    if (p[0] != detail::kMagic[0] || p[1] != detail::kMagic[1] || p[2] != detail::kMagic[2]) return r;
    if (p[3] > detail::kFormat) { r.verdict = Verdict::newerFormat; return r; }
    if (p[3] != detail::kFormat) return r;
    Key k;
    k.kind         = p[4];
    k.number       = (std::uint32_t { p[5] } << 24) | (std::uint32_t { p[6] } << 16) | (std::uint32_t { p[7] } << 8) | p[8];
    k.issued       = static_cast<std::uint16_t> ((p[9] << 8) | p[10]);
    k.updatesUntil = static_cast<std::uint16_t> ((p[11] << 8) | p[12]);
    std::size_t at = 13;
    auto field = [&] (std::string& out, std::size_t max) -> bool
    {
        if (at >= signedSize) return false;
        const std::size_t n = p[at++];
        if (n > max || at + n > signedSize) return false;
        out.assign (reinterpret_cast<const char*> (p + at), n);
        at += n;
        return true;
    };
    if (! field (k.product, detail::kMaxProduct) || ! field (k.name, detail::kMaxName) || ! field (k.email, detail::kMaxEmail)
        || at != signedSize || ! detail::productIdOk (k.product) || ! detail::utf8Ok (k.name) || ! detail::utf8Ok (k.email))
        return r;

    r.key = std::move (k);
    r.verdict = (r.key.product == product) ? Verdict::valid : Verdict::otherProduct;
    r.block = text (*bytes, r.key.product);   // the key alone, as it is issued: what a store keeps
    return r;
}

} // namespace felitronics::appkit::licence

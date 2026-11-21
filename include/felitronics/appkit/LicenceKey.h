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

// The base32 characters a person pasted: between a BEGIN line and the next END line when there is a BEGIN line
// (anything around them is the e-mail's), else the whole text. Spaces, line breaks and a mail client's '>' quote marks
// are dropped and lower case is raised; any other character makes the paste unreadable.
inline std::optional<std::string> extract (std::string_view text)
{
    std::vector<std::string_view> lines;
    for (std::size_t start = 0; start <= text.size();)
    {
        auto end = text.find ('\n', start);
        if (end == std::string_view::npos) end = text.size();
        lines.push_back (text.substr (start, end - start));
        start = end + 1;
    }
    auto trimmed = [] (std::string_view l)
    {
        while (! l.empty() && (l.front() == ' ' || l.front() == '\t' || l.front() == '>' || l.front() == '\r')) l.remove_prefix (1);
        while (! l.empty() && (l.back() == ' ' || l.back() == '\t' || l.back() == '\r')) l.remove_suffix (1);
        return l;
    };
    auto opens = [] (std::string_view l, std::string_view word)   // "-----BEGIN" / "-----END", any case
    {
        if (l.size() < 5 + word.size() || l.substr (0, 5) != "-----") return false;
        for (std::size_t i = 0; i < word.size(); ++i)
        {
            char c = l[5 + i];
            if (c >= 'a' && c <= 'z') c = static_cast<char> (c - 'a' + 'A');
            if (c != word[i]) return false;
        }
        return true;
    };
    std::size_t from = 0, to = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (opens (trimmed (lines[i]), "BEGIN"))
        {
            from = i + 1; to = lines.size();
            for (std::size_t j = i + 1; j < lines.size(); ++j)
                if (opens (trimmed (lines[j]), "END")) { to = j; break; }
            if (to == lines.size()) return std::nullopt;   // a BEGIN without its END: cut short
            break;
        }
    std::string out;
    for (std::size_t i = from; i < to; ++i)
        for (char c : trimmed (lines[i]))
        {
            if (c == ' ' || c == '\t') continue;
            if (c >= 'a' && c <= 'z') c = static_cast<char> (c - 'a' + 'A');
            if ((c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7')) out += c;
            else return std::nullopt;
        }
    if (out.empty()) return std::nullopt;
    return out;
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
    const auto chars = detail::extract (pasted);
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
    return r;
}

} // namespace felitronics::appkit::licence

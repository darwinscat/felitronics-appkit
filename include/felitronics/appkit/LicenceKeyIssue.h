// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

// LicenceKeyIssue.h — making keys: the private half of LicenceKey.h, for the tests and the tool that issues keys.
// A product never includes this: it holds the public key only. The seed (the private key) is never in a repository.

#include <felitronics/appkit/LicenceKey.h>

#include <monocypher.h>

namespace felitronics::appkit::licence
{
/** The public key a product carries, for the seed that signs its keys. */
inline PublicKey publicKeyOf (const Seed& seed)
{
    Seed s = seed;                         // Monocypher wipes the seed it is given
    std::array<std::uint8_t, 64> secret {};
    PublicKey pk {};
    crypto_ed25519_key_pair (secret.data(), pk.data(), s.data());
    crypto_wipe (secret.data(), secret.size());
    return pk;
}

/** Any bytes, signed: the bytes followed by their 64-byte Ed25519 signature. */
inline Bytes sign (const Bytes& bytes, const Seed& seed)
{
    Seed s = seed;
    std::array<std::uint8_t, 64> secret {};
    PublicKey pk {};
    crypto_ed25519_key_pair (secret.data(), pk.data(), s.data());
    std::array<std::uint8_t, 64> sig {};
    crypto_ed25519_sign (sig.data(), secret.data(), bytes.data(), bytes.size());
    crypto_wipe (secret.data(), secret.size());
    Bytes out = bytes;
    out.insert (out.end(), sig.begin(), sig.end());
    return out;
}

/** The key's block, ready for the e-mail; nullopt when a field breaks its limits (see payload()). */
inline std::optional<std::string> issue (const Key& k, const Seed& seed)
{
    const auto p = payload (k);
    if (! p) return std::nullopt;
    return text (sign (*p, seed), k.product);
}
} // namespace felitronics::appkit::licence

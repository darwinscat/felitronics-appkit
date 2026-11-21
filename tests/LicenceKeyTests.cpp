// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// LicenceKey.h against things it did not make. Ed25519 against RFC 8032's own test vectors. The key format against
// three keys built from docs/licence-key.md by Python (struct, base64) and OpenSSL's Ed25519 — no appkit code — which
// this code must read field for field and issue byte for byte. Then the promises: every changed character and every
// flipped bit is refused; a paste survives what mail clients do and nothing more; a key signed by anyone else, for
// another product, in a newer format, or with a non-canonical signature is told apart; the fields' limits hold; a key
// covers every version released up to its day and none after; the trial counts calendar days from the first run.

#include <felitronics/appkit/LicenceKey.h>
#include <felitronics/appkit/LicenceKeyIssue.h>

#include <cstdio>
#include <string>

using namespace felitronics::appkit::licence;

static int checks = 0, failures = 0;
static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}
static void group (const char* name) { std::printf ("  - %s\n", name); }

static Bytes hex (std::string_view h)
{
    Bytes b;
    auto nib = [] (char c) { return static_cast<std::uint8_t> (c <= '9' ? c - '0' : c - 'a' + 10); };
    for (std::size_t i = 0; i + 1 < h.size(); i += 2) b.push_back (static_cast<std::uint8_t> ((nib (h[i]) << 4) | nib (h[i + 1])));
    return b;
}
template <std::size_t N> static std::array<std::uint8_t, N> arr (std::string_view h)
{
    std::array<std::uint8_t, N> a {};
    const auto b = hex (h);
    for (std::size_t i = 0; i < N && i < b.size(); ++i) a[i] = b[i];
    return a;
}

// ---- the oracle: Python + OpenSSL (scratch script, its output pasted here), seed = SHA-256 of a TEST phrase ----
static const Seed      kTestSeed = arr<32> ("c6cd52fd8cc22c76f043df0948ed0ff5d94fb84a9264ebea04e6b321f86fbf9b");
static const PublicKey kTestPub  = arr<32> ("22a7175c01e755df9f4323631b23b44fdcf8104a008cfab9e29b4a16f59fbb65");
static const char* const kVecIvan =
    "-----BEGIN ORBIT480 KEY-----\n"
    "IRBUWAIAAAAAAKSRFFJJMCDPOJRGS5BUHAYAWSLW\n"
    "MFXCAUDFORZG65QQNF3GC3SAMV4GC3LQNRSS4Y3P\n"
    "NVAERC3QVDMG3UEE4BVVLSBK2M4EEXA22LOURIN5\n"
    "RDKTOF7XYJNKIBONI5NHMTWV6I554OPZVWPS734M\n"
    "HTWII6IOOJGFIGQMZK6LV2YJ\n"
    "-----END ORBIT480 KEY-----\n";
static const char* const kVecJose =
    "-----BEGIN ORBIT480 KEY-----\n"
    "IRBUWAIBAAAAAB2RFFJJMCDPOJRGS5BUHAYBKSTP\n"
    "OPB2SICNYO6GY3DFOIW4HGDSON2GKZAQNJXXGZKA\n"
    "MV4GC3LQNRSS433SM66Z5JD3CIZK265SAUPOBK46\n"
    "NG7V2FX3QKR52WVRFJDZ7IDOFPCA5OXOVUQYJSRE\n"
    "YTNNAUTIRF4ZCK3HMLJOS2IFRBDGPBWP4PV2FWYB\n"
    "-----END ORBIT480 KEY-----\n";
static const char* const kVecGift =
    "-----BEGIN ORBIT-AMP KEY-----\n"
    "IRBUWAIF7777772RFH776CLPOJRGS5BNMFWXABSO\n"
    "N5RG6ZDZAB3SZVZK435HSF2YEKOPSKWYKHSYNPDO\n"
    "OZPTXZJ3SZBYBOAJJCBEBYKCPHJT4WJPSYVQA67S\n"
    "GWNXRCT5VHPQKJWA22N7NVAYOXOXKLYI\n"
    "-----END ORBIT-AMP KEY-----\n";
static constexpr std::uint16_t kDay = 20777;   // 2026-11-20, from Python's datetime

static Key ivan()  { return { "orbit480", "Ivan Petrov", "ivan@example.com", 42, kDay, kDay + 365, kind::bought }; }
static Key jose()  { return { "orbit480", "Jos\xc3\xa9 M\xc3\xbcller-\xc3\x98rsted", "jose@example.org", 7, kDay, kDay + 365, kind::student }; }
static Key gift()  { return { "orbit-amp", "Nobody", "", 4294967295u, kDay, kForever, kind::gift }; }

static std::string b32Of (std::string_view block) { return *detail::extract (block); }

// Adds the group order L to the S half of a signature (little-endian): the same signature, non-canonical.
static void addL (std::uint8_t* s)
{
    static const auto L = hex ("edd3f55c1a631258d69cf7a2def9de1400000000000000000000000000000010");
    unsigned carry = 0;
    for (std::size_t i = 0; i < 32; ++i)
    {
        const unsigned v = s[i] + L[i] + carry;
        s[i] = static_cast<std::uint8_t> (v & 0xffu);
        carry = v >> 8;
    }
}

int main()
{
    std::printf ("LicenceKey — signed, offline product keys\n");

    group ("Ed25519 against RFC 8032's test vectors 1-3 (Monocypher 4.0.2)");
    {
        struct V { const char* sk; const char* pk; const char* msg; const char* sig; };
        const V vs[] = {
            { "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60", "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
              "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b" },
            { "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb", "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
              "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00" },
            { "c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7", "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
              "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a" } };
        int n = 0;
        for (const auto& v : vs)
        {
            ++n;
            const auto seed = arr<32> (v.sk);
            const auto msg = hex (v.msg);
            const auto want = hex (v.sig);
            ok (publicKeyOf (seed) == arr<32> (v.pk), "TEST " + std::to_string (n) + ": the public key from the secret");
            const auto signedMsg = sign (msg, seed);
            ok (Bytes (signedMsg.end() - 64, signedMsg.end()) == want, "TEST " + std::to_string (n) + ": the signature, bit for bit");
            const auto pk = arr<32> (v.pk);
            ok (crypto_ed25519_check (want.data(), pk.data(), msg.data(), msg.size()) == 0, "TEST " + std::to_string (n) + ": checks");
            auto bad = want; bad[10] ^= 0x01;
            ok (crypto_ed25519_check (bad.data(), pk.data(), msg.data(), msg.size()) != 0, "TEST " + std::to_string (n) + ": a flipped bit fails");
        }
    }

    group ("the reference keys (Python + OpenSSL from docs/licence-key.md): read field for field, issued byte for byte");
    {
        ok (publicKeyOf (kTestSeed) == kTestPub, "the test seed's public key is OpenSSL's");
        struct C { const char* text; Key key; const char* product; };
        const C cs[] = { { kVecIvan, ivan(), "orbit480" }, { kVecJose, jose(), "orbit480" }, { kVecGift, gift(), "orbit-amp" } };
        for (const auto& c : cs)
        {
            const auto r = read (c.text, kTestPub, c.product);
            ok (r.verdict == Verdict::valid, c.key.name + ": valid");
            ok (r.key == c.key, c.key.name + ": every field as written");
            const auto issued = issue (c.key, kTestSeed);
            ok (issued && *issued == c.text, c.key.name + ": issued here = OpenSSL's block, character for character");
        }
    }

    group ("every changed character and every flipped bit is refused");
    {
        const auto b32 = b32Of (kVecIvan);
        int accepted = 0, tried = 0;
        for (std::size_t i = 0; i < b32.size(); ++i)
            for (char c : std::string_view (detail::kAlphabet, 32))
            {
                if (c == b32[i]) continue;
                auto t = b32; t[i] = c; ++tried;
                if (read (t, kTestPub, "orbit480").verdict != Verdict::damaged) ++accepted;
            }
        ok (tried == static_cast<int> (b32.size()) * 31 && accepted == 0,
            std::to_string (accepted) + " of " + std::to_string (tried) + " one-character changes accepted");

        // The reference keys are all whole multiples of 5 bytes, so their last character carries no spare bits. One
        // byte more leaves 2 spare bits in the last character: changing only those must be refused too.
        auto padded = ivan(); padded.name += "!";
        const auto p32 = b32Of (*issue (padded, kTestSeed));
        ok (detail::unbase32 (p32)->size() % 5 != 0, "the padded key really has spare bits");
        int pAccepted = 0, pTried = 0;
        for (std::size_t i = 0; i < p32.size(); ++i)
            for (char c : std::string_view (detail::kAlphabet, 32))
            {
                if (c == p32[i]) continue;
                auto t = p32; t[i] = c; ++pTried;
                if (read (t, kTestPub, "orbit480").verdict != Verdict::damaged) ++pAccepted;
            }
        ok (pAccepted == 0, std::to_string (pAccepted) + " of " + std::to_string (pTried) + " one-character changes accepted (key with spare bits)");
        const auto bytes = *detail::unbase32 (b32);
        int bitsAccepted = 0;
        for (std::size_t i = 0; i < bytes.size() * 8; ++i)
        {
            auto b = bytes; b[i / 8] ^= static_cast<std::uint8_t> (1u << (i % 8));
            if (read (detail::base32 (b), kTestPub, "orbit480").verdict != Verdict::damaged) ++bitsAccepted;
        }
        ok (bitsAccepted == 0, std::to_string (bitsAccepted) + " of " + std::to_string (bytes.size() * 8) + " flipped bits accepted");
        ok (read (b32.substr (0, b32.size() - 1), kTestPub, "orbit480").verdict == Verdict::damaged, "one character short");
        ok (read (b32 + "A", kTestPub, "orbit480").verdict == Verdict::damaged, "one character more");
        ok (read (b32 + "AAAAAAAA", kTestPub, "orbit480").verdict == Verdict::damaged, "five zero bytes more");
    }

    group ("a paste: what mail clients do is fine, nothing else is");
    {
        auto valid = [] (const std::string& t) { return read (t, kTestPub, "orbit480").verdict == Verdict::valid; };
        const std::string k = kVecIvan, b32 = b32Of (kVecIvan);
        std::string lower; for (char c : k) lower += (c >= 'A' && c <= 'Z') ? static_cast<char> (c - 'A' + 'a') : c;
        ok (valid (lower), "lower case (the BEGIN line too)");
        std::string crlf; for (char c : k) { if (c == '\n') crlf += '\r'; crlf += c; }
        ok (valid (crlf), "CRLF line ends");
        std::string quoted = "> "; for (char c : k) { quoted += c; if (c == '\n') quoted += "> "; }
        ok (valid (quoted), "quoted with '> ' by a reply");
        ok (valid ("Hello Ivan,\n\nhere is your key:\n\n" + k + "\nThank you! (c) 2026\n"), "inside a letter, around the BEGIN/END lines");
        ok (valid (b32), "the bare base32, no BEGIN/END");
        std::string spaced; for (std::size_t i = 0; i < b32.size(); ++i) { spaced += b32[i]; if (i % 4 == 3) spaced += ' '; }
        ok (valid (spaced), "spaces inside");
        ok (valid (k + "\n" + kVecJose), "two keys: the first is read");
        ok (! valid (k.substr (0, k.find ("-----END"))), "BEGIN without END: cut short");
        ok (! valid ("Here is your key: " + b32), "letter text without BEGIN/END around the key");
        auto withChar = [&] (char c) { auto t = k; t.insert (t.find ('\n') + 5, 1, c); return t; };
        ok (! valid (withChar ('0')) && ! valid (withChar ('1')) && ! valid (withChar ('8')) && ! valid (withChar ('=')),
            "0, 1, 8 or '=' inside the block");
        ok (read ("", kTestPub, "orbit480").verdict == Verdict::damaged, "an empty paste");
        ok (read ("-----BEGIN ORBIT480 KEY-----\n-----END ORBIT480 KEY-----\n", kTestPub, "orbit480").verdict == Verdict::damaged, "an empty block");
    }

    group ("a paste, the way people paste: several keys, a PGP-signed letter, one flattened line, CR, Unicode spaces, '|' quotes");
    {
        auto verdictFor = [] (const std::string& t, const char* product) { return read (t, kTestPub, product); };
        const std::string k = kVecIvan, amp = kVecGift;
        const auto ivanForOrbit = verdictFor (amp + "\n" + k, "orbit480");
        ok (ivanForOrbit.verdict == Verdict::valid && ivanForOrbit.key == ivan(), "an OrbitAmp key first, then Orbit480's: Orbit480 reads its own");
        const auto ampForAmp = verdictFor (k + "\n" + amp, "orbit-amp");
        ok (ampForAmp.verdict == Verdict::valid && ampForAmp.key == gift(), "...and OrbitAmp reads its own from the same letter, whatever the order");
        const auto neither = verdictFor (amp + "\n" + k, "orbit-pedal");
        ok (neither.verdict == Verdict::otherProduct && neither.key.product == "orbit-amp", "a letter naming neither product: the first block");
        ok (verdictFor ("-----BEGIN PGP SIGNED MESSAGE-----\nHash: SHA256\n\nYour key:\n" + k
                            + "-----BEGIN PGP SIGNATURE-----\niQEzBAEBCAAdFiEE\n-----END PGP SIGNATURE-----\n", "orbit480").verdict == Verdict::valid,
            "inside a PGP-signed letter: the PGP lines are not a key's");
        std::string flat; for (char c : k) flat += c == '\n' ? ' ' : c;
        ok (verdictFor ("Your key: " + flat + " Thanks!", "orbit480").verdict == Verdict::valid, "a block flattened to one line, spaces for breaks");
        std::string cr; for (char c : k) cr += c == '\n' ? '\r' : c;
        ok (verdictFor (cr, "orbit480").verdict == Verdict::valid, "CR-only line ends (classic Mac)");
        std::string nbsp; for (char c : k) { nbsp += c; if (c == '\n') nbsp += "\xc2\xa0\xe2\x80\x89"; }
        nbsp.insert (nbsp.find ('\n') + 12, "\xe2\x80\x8b\xe3\x80\x80\xef\xbb\xbf");
        ok (verdictFor (nbsp, "orbit480").verdict == Verdict::valid, "no-break, thin, zero-width and ideographic spaces and a BOM are spaces");
        std::string piped = "| "; for (char c : k) { piped += c; if (c == '\n') piped += "| > "; }
        ok (verdictFor (piped, "orbit480").verdict == Verdict::valid, "quoted with '|' (and '|' then '>')");
        std::string inner = k; inner.insert (inner.find ('\n') + 9, "|");
        ok (verdictFor (inner, "orbit480").verdict == Verdict::damaged, "a '|' inside a line is no quote mark: damaged");
        ok (verdictFor (flat.substr (0, flat.find ("-----END")), "orbit480").verdict == Verdict::damaged, "a flattened BEGIN with no END: cut short");
        ok (verdictFor ("-----BEGIN ORBIT480 KEYS-----\n" + b32Of (kVecIvan) + "\n-----END ORBIT480 KEYS-----\n", "orbit480").verdict == Verdict::damaged,
            "a near-miss header is not a key's BEGIN, and the lines around the key make it unreadable");
        ok (verdictFor (k + "\n" + amp, "orbit480").block == *issue (ivan(), kTestSeed), "the reading carries the key alone, as it is issued");
        ok (verdictFor (amp, "orbit480").block == *issue (gift(), kTestSeed), "...another product's key too, under its own header");
        ok (verdictFor ("nothing", "orbit480").block.empty(), "a damaged paste carries no block");
    }

    group ("a public key of small order checks nothing (a forgery that is the identity and a zero scalar)");
    {
        // The probe: a payload signed with R = the identity point (01 00…) and S = 0. Against such a public key
        // Ed25519's own equation holds for a share of all messages (for the identity key, for every one).
        auto probe = [] (std::uint32_t number)
        {
            auto k = ivan(); k.number = number;
            auto b = *payload (k);
            Bytes sig (64, 0);
            sig[0] = 1;
            b.insert (b.end(), sig.begin(), sig.end());
            return b;
        };
        PublicKey zero {};
        PublicKey identity {}; identity[0] = 1;
        int rawZero = 0, rawIdentity = 0;
        std::uint32_t passingZero = 0;
        for (std::uint32_t n = 0; n < 64; ++n)
        {
            const auto b = probe (n);
            const auto signedSize = b.size() - 64;
            if (crypto_ed25519_check (b.data() + signedSize, zero.data(), b.data(), signedSize) == 0) { if (rawZero++ == 0) passingZero = n; }
            if (crypto_ed25519_check (b.data() + signedSize, identity.data(), b.data(), signedSize) == 0) ++rawIdentity;
        }
        ok (rawZero > 0 && rawIdentity == 64, "the probe is real: Monocypher alone accepts it against the zero key ("
                                                  + std::to_string (rawZero) + " of 64) and the identity key (" + std::to_string (rawIdentity) + " of 64)");
        ok (read (text (probe (passingZero), "orbit480"), zero, "orbit480").verdict == Verdict::damaged, "read() against the all-zero key: damaged");
        ok (read (text (probe (0), "orbit480"), identity, "orbit480").verdict == Verdict::damaged, "read() against the identity key: damaged");
        const char* const smallOrder[] {
            "0000000000000000000000000000000000000000000000000000000000000000", "0100000000000000000000000000000000000000000000000000000000000000",
            "26e8958fc2b227b045c3f489f2ef98f0d5dfac05d3c63339b13802886d53fc05", "c7176a703d4dd84fba3c0b760d10670f2a2053fa2c39ccc64ec7fd7792ac037a",
            "ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f", "edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
            "eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f" };
        int refused = 0;
        for (const char* h : smallOrder)
        {
            auto pk = arr<32> (h);
            if (read (kVecIvan, pk, "orbit480").verdict == Verdict::damaged) ++refused;
            pk[31] ^= 0x80;   // the same point's other sign
            if (read (kVecIvan, pk, "orbit480").verdict == Verdict::damaged) ++refused;
        }
        ok (refused == 14, "libsodium's seven small-order encodings, either sign: all damaged (" + std::to_string (refused) + " of 14)");
        ok (read (kVecIvan, kTestPub, "orbit480").verdict == Verdict::valid, "the real public key still reads the real key");
    }

    group ("the e-mail field: well-formed UTF-8, at most 120 bytes, even when signed by us");
    {
        // Hand-made payloads (payload() refuses these fields), signed with the test seed: only read() stands between
        // such bytes and the screen.
        auto raw = [] (std::string_view email)
        {
            Bytes b (detail::kMagic.begin(), detail::kMagic.end());
            b.push_back (detail::kFormat);
            b.push_back (kind::bought);
            detail::put32 (b, 42);
            detail::put16 (b, kDay);
            detail::put16 (b, static_cast<std::uint16_t> (kDay + 365));
            detail::putText (b, "orbit480");
            detail::putText (b, "Ivan Petrov");
            b.push_back (static_cast<std::uint8_t> (email.size()));
            b.insert (b.end(), email.begin(), email.end());
            return text (sign (b, kTestSeed), "orbit480");
        };
        ok (read (raw ("ivan@example.com"), kTestPub, "orbit480").verdict == Verdict::valid, "the hand-made payload reads when its e-mail is fine");
        ok (read (raw (std::string (120, 'e')), kTestPub, "orbit480").verdict == Verdict::valid, "120 bytes: the limit, still valid");
        ok (read (raw (std::string (121, 'e')), kTestPub, "orbit480").verdict == Verdict::damaged, "121 bytes: damaged");
        ok (read (raw ("iv\xff" "an@example.com"), kTestPub, "orbit480").verdict == Verdict::damaged, "a byte that is no UTF-8: damaged");
        ok (read (raw ("\xc0\xafvan@example.com"), kTestPub, "orbit480").verdict == Verdict::damaged, "an overlong '/': damaged");
        ok (read (raw ("\xed\xa0\x80@example.com"), kTestPub, "orbit480").verdict == Verdict::damaged, "a surrogate: damaged");
    }

    group ("whose key: another product's, someone else's signature, a newer format");
    {
        const auto amp = read (kVecGift, kTestPub, "orbit480");
        ok (amp.verdict == Verdict::otherProduct && amp.key.product == "orbit-amp", "a genuine OrbitAmp key, read by Orbit480, says whose it is");
        Seed other = kTestSeed; other[0] ^= 0x80;
        ok (read (*issue (ivan(), other), kTestPub, "orbit480").verdict == Verdict::damaged, "signed by another seed: damaged");
        ok (read (kVecIvan, publicKeyOf (other), "orbit480").verdict == Verdict::damaged, "checked with another public key: damaged");

        auto forged = [] (auto edit) { auto p = *payload (ivan()); edit (p); return text (sign (p, kTestSeed), "orbit480"); };
        ok (read (forged ([] (Bytes& p) { p[3] = 2; }), kTestPub, "orbit480").verdict == Verdict::newerFormat, "format 2: a newer format");
        ok (read (forged ([] (Bytes& p) { p[3] = 0; }), kTestPub, "orbit480").verdict == Verdict::damaged, "format 0: damaged");
        ok (read (forged ([] (Bytes& p) { p[0] = 'X'; }), kTestPub, "orbit480").verdict == Verdict::damaged, "another magic: damaged");
        ok (read (forged ([] (Bytes& p) { p.push_back (0); }), kTestPub, "orbit480").verdict == Verdict::damaged, "a byte after the e-mail (signed): damaged");
        ok (read (forged ([] (Bytes& p) { p.pop_back(); }), kTestPub, "orbit480").verdict == Verdict::damaged, "the e-mail one byte short (signed): damaged");
        ok (read (forged ([] (Bytes& p) { p[13] = 200; }), kTestPub, "orbit480").verdict == Verdict::damaged, "a product length past the end (signed): damaged");
        ok (read (forged ([] (Bytes& p) { p[14] = 'O'; }), kTestPub, "orbit480").verdict == Verdict::damaged, "an upper-case product id (signed): damaged");
        ok (read (forged ([] (Bytes& p) { p[24] = 0xff; }), kTestPub, "orbit480").verdict == Verdict::damaged, "a name that is not UTF-8 (signed): damaged");
        auto longField = [] (std::size_t productLen, std::size_t nameLen)   // a signed key past the limits payload() keeps
        {
            Bytes p (detail::kMagic.begin(), detail::kMagic.end());
            p.push_back (detail::kFormat); p.push_back (kind::bought);
            detail::put32 (p, 1); detail::put16 (p, kDay); detail::put16 (p, kDay);
            detail::putText (p, std::string (productLen, 'p'));
            detail::putText (p, std::string (nameLen, 'n'));
            detail::putText (p, "");
            return text (sign (p, kTestSeed), "p");
        };
        ok (read (longField (32, 120), kTestPub, std::string (32, 'p')).verdict == Verdict::valid, "at the limits (signed by hand): valid");
        ok (read (longField (32, 121), kTestPub, std::string (32, 'p')).verdict == Verdict::damaged, "a 121-byte name (signed): damaged");
        ok (read (longField (33, 0), kTestPub, std::string (33, 'p')).verdict == Verdict::damaged, "a 33-character product (signed): damaged");
        const auto n = read (forged ([] (Bytes& p) { p[4] = 77; }), kTestPub, "orbit480");
        ok (n.verdict == Verdict::valid && n.key.kind == 77, "an unknown kind: still a licence, its number kept");

        auto bytes = *detail::unbase32 (b32Of (kVecIvan));
        addL (bytes.data() + bytes.size() - 32);
        ok (read (detail::base32 (bytes), kTestPub, "orbit480").verdict == Verdict::damaged, "the same signature with S + L (non-canonical): damaged");
    }

    group ("the fields' limits");
    {
        auto k = ivan();
        k.name.assign (120, 'n'); k.email.assign (120, 'e'); k.product.assign (32, 'p');
        const auto t = issue (k, kTestSeed);
        ok (t && read (*t, kTestPub, k.product).key == k, "120-byte name and e-mail, 32-character product: round trip");
        k = ivan(); k.name.assign (121, 'n');  ok (! payload (k), "a 121-byte name is refused");
        k = ivan(); k.email.assign (121, 'e'); ok (! payload (k), "a 121-byte e-mail is refused");
        k = ivan(); k.product.assign (33, 'p'); ok (! payload (k), "a 33-character product is refused");
        k = ivan(); k.product = "";            ok (! payload (k), "an empty product is refused");
        k = ivan(); k.product = "Orbit480";    ok (! payload (k), "an upper-case product is refused");
        k = ivan(); k.name = "\xc0\xaf";       ok (! payload (k), "an overlong UTF-8 name is refused");
        k = ivan(); k.name = "\xed\xa0\x80";   ok (! payload (k), "a UTF-16 surrogate in UTF-8 is refused");
        k = ivan(); k.name = "abc\xe2\x82";    ok (! payload (k), "a name cut inside a character is refused");
        k = ivan(); k.name = "\xf0\x9f\x90\x88 Cat"; k.email = "";
        const auto c = issue (k, kTestSeed);
        ok (c && read (*c, kTestPub, "orbit480").key == k, "a four-byte character in the name, an empty e-mail: round trip");
    }

    group ("updates: every version released up to updatesUntil, none after; forever is forever");
    {
        const auto k = ivan();
        ok (covers (k, kDay) && covers (k, kDay + 365) && covers (k, 0), "released on the purchase day, the last day, or long before: covered");
        ok (! covers (k, kDay + 366), "released the day after the year: not covered");
        ok (covers (gift(), 0xfffe), "a forever key covers the last day there is");
    }

    group ("where the product stands");
    {
        const std::int32_t first = 21000;
        const auto release = static_cast<std::uint16_t> (21100);
        const auto s0 = status (std::nullopt, first, first, release);
        ok (s0.state == Status::State::trial && s0.trialDaysLeft == 30, "day one: trial, 30 days left");
        const auto s29 = status (std::nullopt, first, first + 29, release);
        ok (s29.state == Status::State::trial && s29.trialDaysLeft == 1, "day 30: trial, 1 day left");
        ok (status (std::nullopt, first, first + 30, release).state == Status::State::demo, "day 31: the demo");
        ok (status (std::nullopt, first, first + 5000, release).state == Status::State::demo, "years later: still the demo, nothing more");
        const auto back = status (std::nullopt, first, first - 400, release);
        ok (back.state == Status::State::trial && back.trialDaysLeft == 30, "a clock before the first run: the first day");
        ok (status (ivan(), first, first, static_cast<std::uint16_t> (kDay + 365)).state == Status::State::licensed, "a key covering this release: licensed, even on day one");
        ok (status (ivan(), first, first, static_cast<std::uint16_t> (kDay + 366)).state == Status::State::updatesOver, "a key whose year ended before this release: updates over, at once");
        ok (status (gift(), first, first + 99999, 0xfffe).state == Status::State::licensed, "a forever key: licensed for every release");
    }

    group ("days from the calendar (the values from Python's datetime)");
    {
        ok (dayFromCivil (1970, 1, 1) == 0, "1970-01-01 = 0");
        ok (dayFromCivil (2026, 11, 20) == 20777, "2026-11-20 = 20777");
        ok (dayFromCivil (2000, 2, 29) == 11016 && dayFromCivil (2000, 3, 1) == 11017, "the leap day of 2000");
        ok (dayFromCivil (2100, 3, 1) - dayFromCivil (2100, 2, 28) == 1, "2100 is not a leap year");
        ok (dayFromCivil (2149, 6, 5) == 65534, "2149-06-05 = 65534, the last day before kForever");
    }

    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

# Licence keys, format 1

The bytes of a paid product's key, for whoever issues keys (the site) and whoever checks them
(`include/felitronics/appkit/LicenceKey.h`). Both sides must agree to the byte: the test vectors at the end are
the check.

## What a key is

A few fields, signed with **Ed25519 as RFC 8032 defines it** (pure Ed25519, SHA-512, no context, no prehash),
written as base32. The product holds the 32-byte public key and checks the signature itself, offline. The seed
(the 32-byte private key) exists only where keys are issued.

## The signed bytes

All numbers big-endian. Text is UTF-8, well formed (no overlong forms, no surrogates), prefixed by its length in
bytes as one byte.

| offset | size | field | value |
|---|---|---|---|
| 0 | 3 | magic | `44 43 4B` ("DCK") |
| 3 | 1 | format | `01` |
| 4 | 1 | kind | 0 bought, 1 student, 2 musician short on money, 3 posted a link, 4 sent their music, 5 gift; a number never changes meaning once issued |
| 5 | 4 | number | the key's number |
| 9 | 2 | issued | the purchase (or gift) day: days since 1970-01-01 UTC |
| 11 | 2 | updatesUntil | the last release day the key covers; `FFFF` = every version |
| 13 | 1 + P | product | P = 1..32, characters `a-z 0-9 -` only (`orbit480`, `orbit-amp`) |
| … | 1 + N | name | N = 0..120 bytes |
| … | 1 + E | e-mail | E = 0..120 bytes |

Nothing follows the e-mail. The key is these bytes followed by the 64-byte Ed25519 signature of exactly these bytes.

A bought key's `updatesUntil` is `issued + 365`: every version released within a year after the purchase works with
it forever (the product compares its own release day, baked in at its build, never today's date). A renewal is a new
key with a new `issued` and `updatesUntil`.

## The text

RFC 4648 base32, upper case, **no `=` padding**, the spare bits of the last character zero. Lines of 40 characters
between a BEGIN and an END line naming the product in upper case:

```
-----BEGIN ORBIT480 KEY-----
<base32, 40 per line>
-----END ORBIT480 KEY-----
```

A product reads the block whose BEGIN line names it (`-----BEGIN ORBIT480 KEY-----`, any case), or the first key
block when none does, up to the next END line; anything around it is the letter's. Only `-----BEGIN <NAME> KEY-----`
is a key's BEGIN (a `-----BEGIN PGP SIGNED MESSAGE-----` is not), and a block a mail client flattened to one line still
reads. Without a BEGIN line, the whole paste is the key. Line breaks (LF, CRLF, CR), spaces (also no-break and the other
Unicode spaces) and the quote marks `>` and `|` in front of a line are dropped, lower case is raised; any other
character, a BEGIN without an END, a length no byte string has, or spare bits that are not zero make the key "damaged"
before its signature is checked. A public key of small order (libsodium's blacklist) checks nothing: every key is
"damaged" against it.

## What a product answers

| verdict | when |
|---|---|
| valid | the signature checks with the product's public key, the format is 1, the fields keep their limits, the product is this one |
| other product | all of that, for another product: the product names it |
| newer format | the signature checks and the format is above 1: a newer build reads it |
| damaged | everything else |

## Test vectors

Made by Python (`struct`, `base64`) and OpenSSL 3.5 (`pkeyutl -sign -rawin`), not by appkit. The seed is a TEST seed
(SHA-256 of "felitronics-appkit licence TEST seed: never ship"); no product carries its public key.

```
seed   c6cd52fd8cc22c76f043df0948ed0ff5d94fb84a9264ebea04e6b321f86fbf9b
public 22a7175c01e755df9f4323631b23b44fdcf8104a008cfab9e29b4a16f59fbb65
```

1. `orbit480`, "Ivan Petrov", "ivan@example.com", number 42, issued 20777 (2026-11-20), updatesUntil 21142, kind 0:

```
-----BEGIN ORBIT480 KEY-----
IRBUWAIAAAAAAKSRFFJJMCDPOJRGS5BUHAYAWSLW
MFXCAUDFORZG65QQNF3GC3SAMV4GC3LQNRSS4Y3P
NVAERC3QVDMG3UEE4BVVLSBK2M4EEXA22LOURIN5
RDKTOF7XYJNKIBONI5NHMTWV6I554OPZVWPS734M
HTWII6IOOJGFIGQMZK6LV2YJ
-----END ORBIT480 KEY-----
```

2. `orbit480`, "José Müller-Ørsted" (UTF-8), "jose@example.org", number 7, issued 20777, updatesUntil 21142, kind 1:

```
-----BEGIN ORBIT480 KEY-----
IRBUWAIBAAAAAB2RFFJJMCDPOJRGS5BUHAYBKSTP
OPB2SICNYO6GY3DFOIW4HGDSON2GKZAQNJXXGZKA
MV4GC3LQNRSS433SM66Z5JD3CIZK265SAUPOBK46
NG7V2FX3QKR52WVRFJDZ7IDOFPCA5OXOVUQYJSRE
YTNNAUTIRF4ZCK3HMLJOS2IFRBDGPBWP4PV2FWYB
-----END ORBIT480 KEY-----
```

3. `orbit-amp`, "Nobody", empty e-mail, number 4294967295, issued 20777, updatesUntil FFFF, kind 5:

```
-----BEGIN ORBIT-AMP KEY-----
IRBUWAIF7777772RFH776CLPOJRGS5BNMFWXABSO
N5RG6ZDZAB3SZVZK435HSF2YEKOPSKWYKHSYNPDO
OZPTXZJ3SZBYBOAJJCBEBYKCPHJT4WJPSYVQA67S
GWNXRCT5VHPQKJWA22N7NVAYOXOXKLYI
-----END ORBIT-AMP KEY-----
```

`tests/LicenceKeyTests.cpp` reads all three field for field and issues all three character for character.

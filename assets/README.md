# assets

The family's brand kit: the maker's mark and the faces. Everything here is either compiled into
`include/felitronics/appkit/BrandAssets.h` or kept as an original for work that has not started
yet — the table says which, and *How a face reaches a plugin* below says how one becomes the
other.

| file | shipped how | used by |
|---|---|---|
| `catlogo.svg` | inlined in `BrandAssets.h` | every product's header and about box |
| `Michroma-Regular.ttf` | **source** for the base64 in `BrandAssets.h` | the display face of the whole family |
| `Michroma-web.woff2` | copied into a site's static assets by hand | headings on darwinscat.com |
| `ScienceGothic-VF.ttf` | **not shipped in any binary** | nothing yet — see below |
| `Tektur-VF.ttf` | **not shipped in any binary** | nothing yet — see below |

## The two that are only stored

Michroma has no Cyrillic — measured, not assumed: 485 codepoints, none of them Cyrillic, three
Greek and those mathematical. So the face cannot set anything a player types in Russian or
Ukrainian, and one day something will have to.

These two are the candidates that survived measuring all 212 Google Fonts families that have
Cyrillic and are display, sans or mono. They are here so the answer is not searched for twice:

- **Science Gothic** — variable, `wdth 50–200` and `wght 100–900`. At **wdth 146 / wght 250** its
  average uppercase advance sits within **0.3%** of Michroma's. It is the only candidate that can
  be *tuned* to the family's proportions rather than merely stand near them.
- **Tektur** — variable, `wdth 75–100`, `wght 400–900`. The narrow voice: 37% narrower than
  Michroma and a much heavier stroke. A second face, never a replacement.

Both are OFL, both declare no Reserved Font Name, and both ship with their licence beside them.

## How a face reaches a plugin

Nobody installs anything. A product does not ship a font file, a user does not download one,
and the build does not go looking for one. The face is **compiled in**, as text, and JUCE parses
it at startup.

    ScienceGothic-VF.ttf  ──▶  pin the axes  ──▶  cut to the character set  ──▶  base64
      original, HERE           an instancer        a subsetter                     ──▶ BrandAssets.h
      + its licence            221 kB               58 kB                              ships in the binary
                               └────────── scaffolding, kept nowhere ──────────┘

**Two things are stored: the first and the last.** The original, because it is the provenance
and it carries the licence; and the base64 in the header, because that is what actually travels.
The two files in the middle fall out of one command each and mean nothing on their own — storing
them is how a source and a product drift apart, and binaries never leave a git history once they
enter it.

**Who cuts, and when.** A person, by hand, on the day the face changes or the character set does.
That is rare: Michroma's set changed in September 2026 for the first time since it was added.
`fonttools` is needed only by that person, only at that moment — never by a consumer, never by CI.

Two roads were not taken, and knowing why saves proposing them again:

- **Cutting at build time** would put Python, fonttools and brotli in the build of every consumer.
  This library is header-only precisely so that a consumer needs nothing.
- **Storing the cut `.ttf` and embedding it through the consumer's CMake** is what products did
  before September 2026. A product that never wrote that CMake step opened a window in the host's
  system font, and a product that did carried a second copy of a font this library was already
  carrying — both at once, parsed twice, in every binary.

## Regenerating

The embedded face — whole, hinting dropped, then base64'd into the chunk array in `BrandAssets.h`:

    pyftsubset assets/Michroma-Regular.ttf --output-file=michroma-full.ttf \
      --unicodes="*" --layout-features="kern" --no-hinting --desubroutinize --drop-tables+=DSIG

The web copy — cut to what a heading uses, printable ASCII plus `· © – — ’ ° ×`, and only ever
woff2, because that is the one format the web needs now:

    pyftsubset assets/Michroma-Regular.ttf --output-file=Michroma-web.woff2 --flavor=woff2 \
      --unicodes="U+0020-007E,U+00B7,U+00A9,U+2013,U+2014,U+2019,U+00B0,U+00D7" \
      --layout-features="kern" --no-hinting --desubroutinize --drop-tables+=DSIG

The punctuation is not decoration. A heading that picks up a long dash or a curly quote the subset
does not carry falls back to the system font **mid-word**, and two faces in one line is exactly
what a brand face is bought to prevent.

`fonttools` is a Python package, and macOS guards its system Python (PEP 668). Put it somewhere
of its own rather than forcing it in:

    python3 -m venv fontenv && ./fontenv/bin/pip install fonttools brotli

## The set the sites and the panels actually need

darwinscat.com offers twelve languages — **EN DE UK RU CS ES FR IT PL PT RO TR** — and a preset
name is typed by a player, so anything a player might set is the same question. That set is:

    ASCII        U+0020-007E
    Latin-1      U+00A0-00FF     de es fr it pt, and ¿ ¡ « »
    Latin Ext-A  U+0100-017F     cs pl tr, and œ
    Latin Ext-B  U+0218-021B     ro — see below
    Cyrillic     U+0400-045F, U+0490-0491     ru uk, including ґ і ї є
    punctuation  U+2013 U+2014 U+2018 U+2019 U+201C U+201D U+201E U+2026 U+2039 U+203A U+2022
    marks        U+00B7 U+00A9 U+00B0 U+00D7 U+20AC U+2122

**Verify against words, not against ranges.** Two traps cost an afternoon to find, and both would
have reached production looking fine in ten languages out of twelve:

- **Romanian.** The correct `ș` and `ț` are U+0219 and U+021B — comma-below, in Latin Extended-**B**.
  A range that stops at U+017F sets the whole of "Română" and drops two letters into the system
  font mid-word. The cedilla lookalikes at U+015F/U+0163 are a different pair and not what Romanian
  uses.
- **Quotes.** German, Czech and Polish set `„…“` — U+201E and U+201C. `–`, `—` and `’` alone are
  not enough: „Gänsefüßchen“ comes apart.

All three faces here carry every one of those. What was missing was the range, never the glyph —
which is why the check is a sample sentence per language run against the font's `cmap`, not a
reading of the ranges.

Measured on that set, as woff2: **Tektur 8 860 bytes** (all twelve), **Science Gothic at
wdth 146 / wght 300 19 400** (all twelve), **Michroma 11 732** — and Michroma covers ten of the
twelve, because it has no Cyrillic at all. The narrow face carrying every language weighs less
than the wide one carrying ten.

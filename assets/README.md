# assets

The family's brand kit: the maker's mark and the faces. Everything here is compiled into
`include/felitronics/appkit/BrandAssets.h` **or** kept as an original for work that has not
started yet — the table says which.

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

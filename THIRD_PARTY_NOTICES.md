<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->

# Third-party notices

felitronics-appkit is AGPL-3.0-or-later. It is header-only, so most of what a consumer links is
fetched by the consumer, not by this repo — but two third-party assets are **compiled into these
headers** and therefore travel inside every binary that uses them. They are listed first.

| Component | Where it is | License | Notes |
|---|---|---|---|
| **Michroma** (font, subset) | **embedded in `include/felitronics/appkit/BrandAssets.h`** as base64, and shipped whole at `assets/Michroma-Regular.ttf` | SIL OFL 1.1 | © 2011 The Michroma Project Authors (https://github.com/googlefonts/Michroma-font). Licence text at [`assets/Michroma-OFL.txt`](assets/Michroma-OFL.txt). OFL §1 permits embedding in software under another licence (AGPLv3 here); the font itself stays OFL. **The embedded copy is a SUBSET** — see below. |
| **libwebp** | fetched by CMake, **only** with `-DFELITRONICS_APPKIT_WEBP=ON` (default OFF) | BSD-3-Clause | © Google Inc. Pinned by tag like JUCE. A consumer that never asks for `felitronics::appkit_webp` links none of it. |
| **JUCE** 8.0.14 | supplied by the CONSUMER; fetched here only for the test tier | AGPLv3 (our option) | This repo being AGPL + source-public *is* the JUCE compliance — no key, no flag. |

## The embedded Michroma subset

The family's wordmark face used to reach a product only if that product embedded the `.ttf` through
its own CMake. That made the brand optional by accident — a window opened by a product that had not
done so was set in the host's system font — so the face is now carried by the library itself, as
code, and the file in `assets/` remains the full original.

The embedded copy is cut to what the family actually draws: printable ASCII (U+0020–U+007E) plus
`·` `©` `°` `×` `–` `—` `’` — 106 of 498 glyphs, 64 344 → 11 480 bytes. Regenerate it with
[fonttools](https://github.com/fonttools/fonttools):

```
pyftsubset assets/Michroma-Regular.ttf --output-file=michroma-subset.ttf \
  --unicodes="U+0020-007E,U+00B7,U+00A9,U+2013,U+2014,U+2019,U+00B0,U+00D7" \
  --layout-features="kern" --no-hinting --desubroutinize --drop-tables+=DSIG
base64 michroma-subset.ttf     # then split into the chunks BrandAssets.h holds
```

Two OFL points, since a subset is a **Modified Version** of the Font Software:

- **§1 permits it.** Modification and redistribution as part of a software bundle are allowed, with
  or without the original files, provided the copyright notice and this licence travel with it —
  which is what this file and `assets/Michroma-OFL.txt` are for. The subset stays under the OFL; it
  does not become AGPL because the code around it is.
- **The name may stay "Michroma".** The rename obligation in §3 binds only names declared as
  *Reserved Font Names*, and this font's notice declares none — it reads simply
  "Copyright 2011 The Michroma Project Authors". The subset therefore keeps the family name, which
  is also what makes it interchangeable with a product's own copy of the full font.

## The Darwin's Cat mark

`assets/catlogo.svg`, also embedded in `BrandAssets.h`. First-party: © Darwin's Cat, part of this
repo, no third-party licence involved. Listed here only so nobody has to wonder what the other
8 kB blob in that header is.

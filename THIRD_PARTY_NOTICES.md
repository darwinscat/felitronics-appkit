<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->

# Third-party notices

felitronics-appkit is AGPL-3.0-or-later. It is header-only, so most of what a consumer links is
fetched by the consumer, not by this repo — but two third-party assets are **compiled into these
headers** and therefore travel inside every binary that uses them. They are listed first.

| Component | Where it is | License | Notes |
|---|---|---|---|
| **Michroma** (font) | **embedded in `include/felitronics/appkit/BrandAssets.h`** as base64, whole; the original at `assets/Michroma-Regular.ttf`; a cut web copy at `assets/Michroma-web.woff2` | SIL OFL 1.1 | © 2011 The Michroma Project Authors (https://github.com/googlefonts/Michroma-font). Licence text at [`assets/Michroma-OFL.txt`](assets/Michroma-OFL.txt). OFL §1 permits embedding in software under another licence (AGPLv3 here); the font itself stays OFL. |
| **Science Gothic** (font) | `assets/ScienceGothic-VF.ttf` — **stored only**, compiled into nothing | SIL OFL 1.1 | © 2024 Font Detective LLC (https://github.com/googlefonts/science-gothic). Licence text at [`assets/ScienceGothic-OFL.txt`](assets/ScienceGothic-OFL.txt). Kept as a candidate for the Cyrillic the family will need — see [`assets/README.md`](assets/README.md). |
| **Tektur** (font) | `assets/Tektur-VF.ttf` — **stored only**, compiled into nothing | SIL OFL 1.1 | © 2023 The Tektur Project Authors (https://github.com/hyvyys/Tektur). Licence text at [`assets/Tektur-OFL.txt`](assets/Tektur-OFL.txt). The narrow candidate — see [`assets/README.md`](assets/README.md). |
| **libwebp** | fetched by CMake, **only** with `-DFELITRONICS_APPKIT_WEBP=ON` (default OFF) | BSD-3-Clause | © Google Inc. Pinned by tag like JUCE. A consumer that never asks for `felitronics::appkit_webp` links none of it. |
| **JUCE** 8.0.14 | supplied by the CONSUMER; fetched here only for the test tier | AGPLv3 (our option) | This repo being AGPL + source-public *is* the JUCE compliance — no key, no flag. |

## The embedded Michroma

The family's wordmark face used to reach a product only if that product embedded the `.ttf` through
its own CMake. That made the brand optional by accident — a window opened by a product that had not
done so was set in the host's system font — so the face is now carried by the library itself, as
code, and the file in `assets/` remains the untouched original.

The embedded copy is the **whole** font with the hinting and DSIG dropped: 485 codepoints, 491
glyphs, 64 344 → 32 212 bytes. It used to be cut to 106 glyphs, and the cut bought 21 kB in a
plugin that ships neural models while costing a question — "is this character in the set?" — asked
of every label for ever. Regenerate with [fonttools](https://github.com/fonttools/fonttools):

```
pyftsubset assets/Michroma-Regular.ttf --output-file=michroma-full.ttf \
  --unicodes="*" --layout-features="kern" --no-hinting --desubroutinize --drop-tables+=DSIG
base64 michroma-full.ttf       # then split into the chunks BrandAssets.h holds
```

The **web** copy stays cut, because a page pays for every byte before it can paint: printable ASCII
plus `·` `©` `°` `×` `–` `—` `’`, woff2, 5 896 bytes. The command is in [`assets/README.md`](assets/README.md).

Three OFL points, since anything cut or re-flavoured is a **Modified Version** of the Font Software:

- **§1 permits it.** Modification and redistribution as part of a software bundle are allowed, with
  or without the original files, provided the copyright notice and this licence travel with it —
  which is what this file and the `assets/*-OFL.txt` files are for. A modified font stays under the
  OFL; it does not become AGPL because the code around it is.
- **The names may stay.** The rename obligation in §3 binds only names declared as *Reserved Font
  Names*, and none of the three notices declares one — each reads simply "Copyright … Project
  Authors". The copies therefore keep their family names, which is also what makes the embedded
  Michroma interchangeable with a product's own copy of the original.
- **Stored is still distributed.** Science Gothic and Tektur are compiled into nothing, but this
  repository is public, so they travel with it and their notices are owed all the same.

## The Darwin's Cat mark

`assets/catlogo.svg`, also embedded in `BrandAssets.h`. First-party: © Darwin's Cat, part of this
repo, no third-party licence involved. Listed here only so nobody has to wonder what the other
8 kB blob in that header is.

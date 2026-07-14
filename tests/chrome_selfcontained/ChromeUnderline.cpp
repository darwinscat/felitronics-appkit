// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// Self-containment gate: this chrome header must compile as the FIRST (and only) include,
// under the consumer warning set — i.e. it pulls its own dependencies and needs no prelude.
// The header carries all the declarations, so this TU is not empty; the COMPILE is the test.
#include <felitronics/appkit/chrome/ChromeUnderline.h>

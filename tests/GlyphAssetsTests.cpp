// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// The glyph geometry has two readers: JUCE takes DeviceGlyphPaths.h, the website takes
// assets/glyphs/<key>.svg. tools/gen-glyphs.mjs writes both, so they can only disagree if someone
// edits one by hand or regenerates one without the other — this suite is what notices. JUCE-free,
// so it runs in every CI row under the strict flag set.
//
// THEORY. Both outputs carry the same catalogue, so for every key:
//   S1  <key>.svg exists and its <path d="…"> attributes, in order, equal the header's strings;
//   S2  the directory holds no SVG the header does not name (a stale file would be vendored anyway);
//   S3  the file stays inside what both readers take: viewBox 0 0 100 100, only <svg> and <path>
//       elements, no stroke width, no colour, no CSS, no filters or gradients, nothing filled;
//   S4  the path data uses only M, L, C and Z, absolute — the commands the generator emits — so a
//       reader with a smaller SVG-path parser than JUCE's or a browser's still reads every glyph.

#include <felitronics/appkit/DeviceGlyphPaths.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef APPKIT_GLYPH_DIR
 #error "APPKIT_GLYPH_DIR must name assets/glyphs (set by CMakeLists.txt)"
#endif

namespace gp = felitronics::appkit::glyphpaths;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

static std::string readFile (const std::filesystem::path& p)
{
    std::ifstream in (p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string withoutComments (std::string s)
{
    for (auto at = s.find ("<!--"); at != std::string::npos; at = s.find ("<!--", at))
    {
        const auto end = s.find ("-->", at);
        s.erase (at, end == std::string::npos ? std::string::npos : end + 3 - at);
    }
    return s;
}

static std::vector<std::string> pathData (const std::string& svg)
{
    std::vector<std::string> out;
    const std::string open = "<path d=\"";
    for (auto at = svg.find (open); at != std::string::npos; at = svg.find (open, at))
    {
        at += open.size();
        const auto end = svg.find ('"', at);
        if (end == std::string::npos)
            break;
        out.push_back (svg.substr (at, end - at));
        at = end;
    }
    return out;
}

static std::vector<std::string> elementNames (const std::string& svg)
{
    std::vector<std::string> out;
    for (auto at = svg.find ('<'); at != std::string::npos; at = svg.find ('<', at + 1))
    {
        if (at + 1 < svg.size() && svg[at + 1] == '/')
            continue;
        auto end = at + 1;
        while (end < svg.size() && svg[end] != ' ' && svg[end] != '>' && svg[end] != '/' && svg[end] != '\n')
            ++end;
        out.push_back (svg.substr (at + 1, end - at - 1));
    }
    return out;
}

int main()
{
    std::printf ("felitronics::appkit glyph assets tests\n");
    const std::filesystem::path dir (APPKIT_GLYPH_DIR);

    group ("S1: every catalogue glyph has its SVG, and the SVG's paths are the header's");
    for (const auto& g : gp::all)
    {
        const std::string key (g.key);
        const auto file = dir / (key + ".svg");
        ok (std::filesystem::exists (file), key + ".svg exists");
        const auto d = pathData (withoutComments (readFile (file)));
        bool same = d.size() == g.paths.size();
        for (size_t i = 0; same && i < d.size(); ++i)
            same = d[i] == g.paths[i];
        ok (same, key + ": the SVG's path data equals DeviceGlyphPaths.h");
        ok (! g.paths.empty(), key + ": at least one path");
        ok (gp::find (g.key).data() == g.paths.data(), key + ": find() returns its own entry");
    }
    ok (gp::find ("no-such-glyph").empty(), "find() of an unknown key is empty");

    group ("S2: no SVG in the directory that the catalogue does not name");
    {
        std::set<std::string> keys;
        for (const auto& g : gp::all)
            keys.insert (std::string (g.key));
        ok (keys.size() == std::size (gp::all), "catalogue keys are unique");
        for (const auto& e : std::filesystem::directory_iterator (dir))
            if (e.path().extension() == ".svg")
                ok (keys.count (e.path().stem().string()) == 1, e.path().filename().string() + " is in the catalogue");
    }

    group ("S3: the files carry geometry only");
    for (const auto& g : gp::all)
    {
        const std::string key (g.key);
        const auto svg = withoutComments (readFile (dir / (key + ".svg")));
        ok (svg.find ("viewBox=\"0 0 100 100\"") != std::string::npos, key + ": viewBox 0 0 100 100");
        for (const auto& el : elementNames (svg))
            ok (el == "svg" || el == "path", key + ": element <" + el + "> is svg or path");
        for (const char* banned : { "stroke-width", "stroke=", "style", "class=", "filter", "Gradient",
                                    "transform", "opacity", "color", "url(" })
            ok (svg.find (banned) == std::string::npos, key + ": no " + banned);
        for (auto at = svg.find ("fill=\""); at != std::string::npos; at = svg.find ("fill=\"", at + 1))
            ok (svg.compare (at, 11, "fill=\"none\"") == 0, key + ": fill is none");
    }

    group ("S4: path data uses only M, L, C, Z");
    for (const auto& g : gp::all)
        for (const auto d : g.paths)
        {
            bool clean = ! d.empty() && d.front() == 'M';
            for (const char ch : d)
                clean = clean && (ch == 'M' || ch == 'L' || ch == 'C' || ch == 'Z' || ch == ' ' || ch == '.'
                                  || ch == '-' || (ch >= '0' && ch <= '9'));
            ok (clean, std::string (g.key) + ": a path starts with M and uses only M L C Z");
        }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

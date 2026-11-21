// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// appkit_licence_shots <dir> [lang …] — pictures of the licence screens for review: the About window in each
// standing, its two pages, and the after-trial card, in each language asked for (default: en de pl ru). Rendered
// offscreen (createComponentSnapshot); no window is opened and nothing is launched.

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <felitronics/appkit/LicenceKeyIssue.h>
#include <felitronics/appkit/UpdateChecker.h>

#if defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include <felitronics/appkit/VersionBadge.h>
#include <felitronics/appkit/LicenceNotice.h>
#undef private
#if defined (__clang__)
 #pragma clang diagnostic pop
#endif

#include <cstdio>

using namespace felitronics::appkit;
namespace lic = felitronics::appkit::licence;

struct Checker final : UpdateChecker
{
    Checker() : UpdateChecker (config()) {}
    static UpdateChecker::Config config()
    {
        UpdateChecker::Config c;
        c.ownerRepo = "darwinscat/orbit-480";
        c.productName = "Orbit480";
        c.currentVersion = "v0.1.0";
        return c;
    }
};

static void save (juce::Component& c, const juce::File& f)
{
    const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("%s\n", f.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc < 2) { std::printf ("usage: appkit_licence_shots <dir> [lang ...]\n"); return 2; }
    const juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    dir.createDirectory();
    juce::StringArray langs;
    for (int i = 2; i < argc; ++i) langs.add (argv[i]);
    if (langs.isEmpty()) langs = { "en", "de", "pl", "ru" };

    lic::Key ivan;
    ivan.product = "orbit480"; ivan.name = "Ivan Petrov"; ivan.email = "ivan@example.com"; ivan.number = 42;
    ivan.issued = static_cast<std::uint16_t> (lic::dayFromCivil (2026, 11, 20));
    ivan.updatesUntil = static_cast<std::uint16_t> (ivan.issued + 365);

    lic::Standing now;
    Checker chk;
    VersionBadge::Config cfg;
    cfg.productName = "Orbit480";
    cfg.productUrl = "https://darwinscat.com/orbit480";
    cfg.gitHash = "3f2a11c"; cfg.buildNumber = 20261120101500LL;
    cfg.os = "macOS"; cfg.arch = "arm64"; cfg.builder = "ci";
    cfg.licence = "Proprietary";
    cfg.dependencies = { { "felitronics-core", "v0.68.0", "darwinscat/felitronics-core", {}, {} },
                         { "JUCE", "8.0.17", {}, {}, {} } };
    lic::Purchase purchase;
    purchase.standing = [&] { return now; };
    purchase.enterKey = [&] (const juce::String&) { lic::Reading r; r.verdict = lic::Verdict::valid; r.key = ivan; return r; };
    purchase.buyUrl = "https://darwinscat.com/orbit480/buy";
    purchase.renewUrl = "https://darwinscat.com/orbit480/renew";
    purchase.freeLicenceTo = "licences@darwinscat.com";
    purchase.productId = "orbit480";
    cfg.purchase = purchase;

    for (const auto& lang : langs)
    {
        i18n::setLanguage (lang);
        VersionBadge badge (chk, cfg, "VST3");
        const std::pair<const char*, lic::Standing> standings[] {
            { "trial",       { { lic::Status::State::trial, 23 }, std::nullopt } },
            { "demo",        { { lic::Status::State::demo, 0 }, std::nullopt } },
            { "licensed",    { { lic::Status::State::licensed, 0 }, ivan } },
            { "updatesover", { { lic::Status::State::updatesOver, 0 }, ivan } } };
        for (const auto& [name, s] : standings)
        {
            now = s;
            VersionBadge::Panel p (badge, cfg, "VST3", nullptr);
            p.asDialog = true;
            p.closeBtn.setVisible (true);
            p.resized();
            save (p, dir.getChildFile ("about-" + juce::String (name) + "-" + lang + ".png"));
            if (juce::String (name) == "trial")
            {
                p.showPage (VersionBadge::Page::enterKey);
                p.enterKeyPage->box.setText (juce::String (*lic::issue (ivan, lic::Seed {})));
                lic::Reading r; r.verdict = lic::Verdict::otherProduct; r.key = ivan; r.key.product = "orbit-amp";
                p.enterKeyPage->showAnswer (r);
                save (p, dir.getChildFile ("page-enterkey-" + lang + ".png"));
                p.showPage (VersionBadge::Page::freeLicence);
                p.freeLicencePage->choices[1].setToggleState (true, juce::sendNotification);
                p.freeLicencePage->name.setText ("Ivan Petrov");
                p.freeLicencePage->email.setText ("ivan@example.com");
                p.freeLicencePage->update();   // a text change is posted; nobody pumps messages here
                save (p, dir.getChildFile ("page-freelicence-" + lang + ".png"));
            }
        }
        LicenceNotice::Config cc;
        cc.productName = "Orbit480";
        cc.price = juce::String::fromUTF8 ("\xe2\x82\xac" "49");
        LicenceNotice card (cc);
        card.setVisible (true);
        save (card, dir.getChildFile ("card-" + lang + ".png"));
        cc.updatesOver = true;
        cc.updatesUntil = ivan.updatesUntil;
        cc.renewUrl = "https://darwinscat.com/orbit480/renew";
        LicenceNotice renewal (cc);
        renewal.setVisible (true);
        save (renewal, dir.getChildFile ("card-updatesover-" + lang + ".png"));
    }

    // A free product's window, for comparison: the same window it always was.
    i18n::setLanguage ("en");
    auto freeCfg = cfg;
    freeCfg.purchase.reset();
    VersionBadge freeBadge (chk, freeCfg, "VST3");
    VersionBadge::Panel fp (freeBadge, freeCfg, "VST3", nullptr);
    fp.asDialog = true;
    fp.closeBtn.setVisible (true);
    fp.resized();
    save (fp, dir.getChildFile ("about-free-en.png"));
    return 0;
}

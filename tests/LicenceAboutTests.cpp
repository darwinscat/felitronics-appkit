// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// The About window's two halves. A FREE product's window is compared with the window as it was before any of this
// (tests/base/VersionBadgeBase.h, the previous header, built side by side): the same children, bounds, texts and
// pixels. A PAID product's window: the licence block in every standing, its two pages and "Back", the product's own
// function driven with keys issued by the TEST seed, the mailto letter decoded back to its fields, and the
// after-trial card's countdown. Nothing is launched: the screens open addresses through a seam the tests replace.

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <felitronics/appkit/Brand.h>
#include <felitronics/appkit/CallOut.h>
#include <felitronics/appkit/I18n.h>
#include <felitronics/appkit/LicenceKeyIssue.h>
#include <felitronics/appkit/LicencePages.h>
#include <felitronics/appkit/UpdateChecker.h>

#include <functional>
#include <memory>
#include <optional>
#include <typeinfo>
#include <string>
#include <utility>
#include <vector>

#if defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include <felitronics/appkit/VersionBadge.h>
#include <felitronics/appkit/LicenceNotice.h>
#include "base/VersionBadgeBase.h"
#undef private
#if defined (__clang__)
 #pragma clang diagnostic pop
#endif

#include <cstdio>

using namespace felitronics::appkit;
namespace lic = felitronics::appkit::licence;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

static juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

// Field by field: a designated list that leaves members out is a warning on gcc.
static UpdateChecker::Config checkerConfig (juce::String current)
{
    UpdateChecker::Config c;
    c.ownerRepo      = "darwinscat/felitronics-appkit";
    c.productName    = "Orbit480";
    c.currentVersion = std::move (current);
    return c;
}

struct Checker final : UpdateChecker
{
    explicit Checker (juce::String current) : UpdateChecker (checkerConfig (std::move (current))) {}
};

static LicenceNotice::Config cardConfig (const juce::String& price)
{
    LicenceNotice::Config c;
    c.productName = "Orbit480";
    c.price = price;
    return c;
}

// One recipe, two Config types: the base header's and today's.
template <typename Cfg>
static Cfg configOf (int variant)
{
    Cfg cfg;
    cfg.productName = "Orbit480";
    cfg.productUrl  = "https://example.invalid/orbit480?utm_source=gate";
    cfg.gitHash     = "deadbee";
    cfg.buildNumber = 20261120101500LL;
    cfg.buildCount  = 3;
    cfg.os = "macOS"; cfg.arch = "arm64"; cfg.builder = "gate";
    cfg.licence = "AGPL-3.0-or-later";
    cfg.coreVersion = "v0.68.0 (local)";
    cfg.coreOwnerRepo = "darwinscat/felitronics-core";
    cfg.dependencies = { { "JUCE", "8.0.17", {}, "gfeedface", "pin" } };
    if (variant == 1) cfg.notice = "Orbit480 is a trademark of nobody in particular.\n\nA second paragraph, long enough to wrap "
                                   "across the window at least once so the measured height is exercised.";
    if (variant == 2) cfg.feedUrl = {};
    if (variant == 3) cfg.feedPrompt = {};
    if (variant == 4) { cfg.gitDirty = true; cfg.dependencies.clear(); cfg.coreVersion = {}; }
    return cfg;
}

static juce::String colourOf (juce::Component& c, int id) { return c.findColour (id).toDisplayString (true); }

static bool clicks (juce::Component& c)
{
    bool self = false, children = false;
    c.getInterceptsMouseClicks (self, children);
    return self;
}

// Everything a child is, written down: what it is, where, whether it shows, what it says and how.
template <typename PanelT>
static std::string fingerprint (PanelT& p)
{
    p.resized();
    std::string s = "size " + std::to_string (p.getWidth()) + "x" + std::to_string (p.getHeight()) + "\n";
    int i = 0;
    for (auto* c : p.getChildren())
    {
        std::string kind = "component", text, more;
        if (auto* l = dynamic_cast<juce::Label*> (c))
        {
            kind = "label"; text = l->getText().toStdString();
            more = " font " + std::to_string (l->getFont().getHeight()) + " " + colourOf (*l, juce::Label::textColourId).toStdString();
        }
        else if (auto* h = dynamic_cast<juce::HyperlinkButton*> (c))
        {
            kind = "link"; text = h->getButtonText().toStdString();
            more = " " + h->getURL().toString (true).toStdString() + " " + colourOf (*h, juce::HyperlinkButton::textColourId).toStdString();
        }
        else if (auto* t = dynamic_cast<juce::ToggleButton*> (c)) { kind = "toggle"; text = t->getButtonText().toStdString(); more = t->getToggleState() ? " on" : " off"; }
        else if (auto* b = dynamic_cast<juce::TextButton*> (c))   { kind = "textbutton"; text = b->getButtonText().toStdString(); }
        else if (auto* o = dynamic_cast<juce::Button*> (c))       { kind = "button"; text = o->getButtonText().toStdString(); }
        if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (c))
            more += " tip=\"" + tip->getTooltip().toStdString() + "\"";
        s += std::to_string (i++) + " " + kind + (c->isVisible() ? " shown " : " hidden ") + c->getBounds().toString().toStdString()
           + " \"" + text + "\"" + more + (clicks (*c) ? "" : " inert") + "\n";
    }
    return s + "stamp:\n" + p.stampText.toStdString();
}

template <typename PanelT>
static juce::Image render (PanelT& p)
{
    p.resized();
    juce::Image img (juce::Image::ARGB, juce::jmax (1, p.getWidth()), juce::jmax (1, p.getHeight()), true);
    juce::Graphics g (img);
    p.paintEntireComponent (g, true);
    return img;
}

static int differingPixels (const juce::Image& a, const juce::Image& b)
{
    if (a.getWidth() != b.getWidth() || a.getHeight() != b.getHeight()) return -1;
    int n = 0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
            if (a.getPixelAt (x, y) != b.getPixelAt (x, y)) ++n;
    return n;
}

static int inkedPixels (const juce::Image& a)
{
    int n = 0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
            if (a.getPixelAt (x, y).getAlpha() != 0) ++n;
    return n;
}

// What is wrong with a laid-out screen: a piece outside it, two pieces on top of each other, or a one-line control
// whose words are wider than it (they would be cut). Empty paragraphs (an answer not given yet) are skipped.
static std::vector<std::string> layoutFaults (juce::Component& screen)
{
    std::vector<std::string> faults;
    const auto kids = screen.getChildren();
    auto shown = [] (const juce::Component* c)
    {
        const auto* para = dynamic_cast<const lic::Para*> (c);
        return c->isVisible() && ! (para != nullptr && para->getText().isEmpty());
    };
    auto label = [] (const juce::Component* c)
    {
        if (auto* b = dynamic_cast<const juce::Button*> (c)) return b->getButtonText().toStdString();
        if (auto* p = dynamic_cast<const lic::Para*> (c))    return p->getText().substring (0, 30).toStdString();
        return std::string ("a component");
    };
    for (int i = 0; i < kids.size(); ++i)
    {
        if (! shown (kids[i])) continue;
        if (kids[i]->getBounds().isEmpty() || ! screen.getLocalBounds().contains (kids[i]->getBounds()))
            faults.push_back ("outside: " + label (kids[i]));
        for (int j = i + 1; j < kids.size(); ++j)
            if (shown (kids[j]) && kids[i]->getBounds().intersects (kids[j]->getBounds()))
                faults.push_back ("overlap: " + label (kids[i]) + " / " + label (kids[j]));
        if (auto* pill = dynamic_cast<const lic::PillButton*> (kids[i]); pill != nullptr && pill->idealWidth() - 20 > pill->getWidth())
            faults.push_back ("cut: " + label (kids[i]));
        if (auto* tl = dynamic_cast<const lic::TextLink*> (kids[i]); tl != nullptr && tl->idealWidth() - 4 > tl->getWidth())
            faults.push_back ("cut: " + label (kids[i]));
        if (auto* tb = dynamic_cast<const juce::ToggleButton*> (kids[i]);
            tb != nullptr && lic::textWidth (juce::Font (juce::FontOptions (15.0f)), tb->getButtonText()) + 30.0f > (float) tb->getWidth())
            faults.push_back ("cut: " + label (kids[i]));
    }
    return faults;
}

static std::string joined (const std::vector<std::string>& v)
{
    std::string s;
    for (const auto& x : v) s += (s.empty() ? "" : "; ") + x;
    return s;
}

static juce::MouseEvent clickOn (juce::Component& c)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), {}, {}, juce::MouseInputSource::defaultPressure,
                             0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, {}, now, 1, false);
}

// Every text a component tree shows, depth first: what the German pass must have changed.
static void textsOf (juce::Component& c, std::vector<juce::String>& out)
{
    if (auto* p = dynamic_cast<lic::Para*> (&c))                out.push_back (p->getText());
    else if (auto* b = dynamic_cast<juce::Button*> (&c))        out.push_back (b->getButtonText());
    else if (auto* e = dynamic_cast<juce::TextEditor*> (&c))    out.push_back (e->getTextToShowWhenEmpty());
    for (auto* k : c.getChildren())
        if (dynamic_cast<juce::TextEditor*> (&c) == nullptr)   // an editor's own children are its caret and viewport
            textsOf (*k, out);
}

//==============================================================================
// The paid product, as a test plays it: its standing, its stored key, every paste it was handed.
static const lic::Seed kTestSeed = [] {
    lic::Seed s {};
    const char* hex = "c6cd52fd8cc22c76f043df0948ed0ff5d94fb84a9264ebea04e6b321f86fbf9b";
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<std::uint8_t> (juce::String (hex + 2 * i, 2).getHexValue32());
    return s;
}();

struct Product
{
    lic::Standing             now { { lic::Status::State::trial, 23 }, std::nullopt };
    std::vector<juce::String> pasted;
    int                       standingCalls = 0;
    lic::PublicKey            pub = lic::publicKeyOf (kTestSeed);

    lic::Purchase purchase()
    {
        lic::Purchase p;
        p.standing = [this] { ++standingCalls; return now; };
        p.enterKey = [this] (const juce::String& s)
        {
            pasted.push_back (s);
            auto r = lic::read (s.toStdString(), pub, "orbit480");
            if (r.verdict == lic::Verdict::valid)
                now = { { lic::Status::State::licensed, 0 }, r.key };
            return r;
        };
        p.buyUrl        = "https://example.invalid/buy?product=orbit480&ref=about";
        p.renewUrl      = "https://example.invalid/renew?product=orbit480";
        p.freeLicenceTo = "licences@example.invalid";
        p.productId     = "orbit480";
        return p;
    }
};

static lic::Key ivan()
{
    lic::Key k;
    k.product = "orbit480"; k.name = "Ivan Petrov"; k.email = "ivan@example.com"; k.number = 42;
    k.issued = static_cast<std::uint16_t> (lic::dayFromCivil (2026, 11, 20));
    k.updatesUntil = static_cast<std::uint16_t> (k.issued + 365);
    return k;
}

// The decoder the test trusts: its own, written from RFC 3986, sharing nothing with percentEncode.
static std::string percentDecode (const std::string& s, bool& clean)
{
    std::string out;
    clean = true;
    auto hexv = [] (char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
    for (size_t i = 0; i < s.size(); ++i)
    {
        const char c = s[i];
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                             || c == '-' || c == '.' || c == '_' || c == '~';
        if (c == '%' && i + 2 < s.size() + 0 && hexv (s[i + 1]) >= 0 && hexv (s[i + 2]) >= 0)
        {
            out += static_cast<char> (hexv (s[i + 1]) * 16 + hexv (s[i + 2]));
            i += 2;
        }
        else
        {
            if (! unreserved && c != '@') clean = false;   // anything else should have been encoded
            out += c;
        }
    }
    return out;
}

struct Mail { std::string to, subject, body; bool clean = true, shaped = false; };

static Mail decodeMailto (const juce::String& url)
{
    Mail m;
    const std::string s = url.toStdString();
    if (s.rfind ("mailto:", 0) != 0) return m;
    const auto q = s.find ('?');
    if (q == std::string::npos) return m;
    bool c1 = true, c2 = true, c3 = true;
    m.to = percentDecode (s.substr (7, q - 7), c1);
    const auto rest = s.substr (q + 1);
    const auto amp = rest.find ('&');
    if (amp == std::string::npos || rest.rfind ("subject=", 0) != 0 || rest.compare (amp + 1, 5, "body=") != 0) return m;
    m.subject = percentDecode (rest.substr (8, amp - 8), c2);
    m.body    = percentDecode (rest.substr (amp + 6), c3);
    m.clean   = c1 && c2 && c3;
    m.shaped  = true;
    return m;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("felitronics::appkit licence screens tests\n");
    i18n::setLanguage ("en");

    group ("a free product's window is the window it was before: children, bounds, texts, pixels");
    {
        Checker chk ("v0.1.0");
        for (int variant = 0; variant <= 4; ++variant)
            for (int dialog = 0; dialog <= 1; ++dialog)
            {
                VersionBadge nowBadge (chk, configOf<VersionBadge::Config> (variant), "VST3");
                base::VersionBadge oldBadge (chk, configOf<base::VersionBadge::Config> (variant), "VST3");
                VersionBadge::Panel nowP (nowBadge, configOf<VersionBadge::Config> (variant), "VST3", nullptr);
                base::VersionBadge::Panel oldP (oldBadge, configOf<base::VersionBadge::Config> (variant), "VST3", nullptr);
                if (dialog == 1)
                {
                    nowP.asDialog = oldP.asDialog = true;
                    nowP.closeBtn.setVisible (true);
                    oldP.closeBtn.setVisible (true);
                }
                const auto tag = "variant " + std::to_string (variant) + (dialog ? " as About" : " as call-out");
                const auto fNow = fingerprint (nowP), fOld = fingerprint (oldP);
                ok (fNow == fOld, tag + ": the same children, bounds and texts");
                if (fNow != fOld) std::printf ("--- before\n%s\n--- now\n%s\n", fOld.c_str(), fNow.c_str());
                const auto a = render (nowP), b = render (oldP);
                ok (differingPixels (a, b) == 0 && inkedPixels (a) > 1000, tag + ": the same pixels (and some)");

                // ...and the same after each thing an update check can say.
                nowP.onResult ({});
                oldP.onResult ({});
                ok (fingerprint (nowP) == fingerprint (oldP) && differingPixels (render (nowP), render (oldP)) == 0,
                    tag + ": the same after a failed check");
                UpdateChecker::Result up; up.ok = true; up.outdated = false;
                nowP.onResult (up); oldP.onResult (up);
                ok (fingerprint (nowP) == fingerprint (oldP), tag + ": the same after \"up to date\"");
                UpdateChecker::Result news; news.ok = true; news.outdated = true; news.latest = "0.2.0";
                news.url = "https://example.invalid/releases/v0.2.0";
                nowP.onResult (news); oldP.onResult (news);
                ok (fingerprint (nowP) == fingerprint (oldP) && differingPixels (render (nowP), render (oldP)) == 0,
                    tag + ": the same after an update was found");
                nowP.copyStamp(); oldP.copyStamp();
                ok (differingPixels (render (nowP), render (oldP)) == 0, tag + ": the same copy receipt");
            }

        // The corner badge's own tooltip, now a translated template: the same sentence in English.
        VersionBadge nowBadge (chk, configOf<VersionBadge::Config> (0), "VST3");
        base::VersionBadge oldBadge (chk, configOf<base::VersionBadge::Config> (0), "VST3");
        ok (nowBadge.getTooltip() == oldBadge.getTooltip()
                && nowBadge.getTooltip() == u8 ("Orbit480 v0.1.0 (VST3) \xe2\x80\x94 click to check for updates"),
            "the badge's tooltip is the one it was");
    }

    group ("a free product's window speaks German, and keeps its shape");
    {
        Checker chk ("v0.1.0");
        const auto cfg = configOf<VersionBadge::Config> (0);
        VersionBadge badge (chk, cfg, "VST3");
        VersionBadge::Panel en (badge, cfg, "VST3", nullptr);
        i18n::setLanguage ("de");
        VersionBadge::Panel de (badge, cfg, "VST3", nullptr);
        ok (de.getWidth() == en.getWidth() && de.getHeight() == en.getHeight()
                && de.getNumChildComponents() == en.getNumChildComponents(), "the same size and children in German");
        ok (de.check.getButtonText() == "Nach Updates suchen" && de.autoCheck.getButtonText() == "Automatisch, einmal am Tag",
            "the update button and its switch");
        ok (de.note.getText() == u8 ("Fragt GitHub nach der neuesten Version. An uns wird nichts gesendet."), "the default update note");
        ok (de.note.getTooltip().startsWith ("Die Anfrage geht an github.com"), "...and its detail");
        ok (de.feedPrompt.getText() == u8 ("Gef\xc3\xa4llt Ihnen die App?") && de.feed.getButtonText() == "Feed the Cat",
            "the prompt is German, the tip jar keeps its name");
        ok (de.closeBtn.getTooltip() == u8 ("Schlie\xc3\x9f" "en"), "the close cross");
        ok (de.rows[0]->lead.getText() == "Orbit480" && de.licence.getText() == "AGPL-3.0-or-later" && de.link.getButtonText() == "by Darwin's Cat",
            "names, the licence text and the byline stay as they are");
        de.onResult ({});
        ok (de.result.getText() == u8 ("Pr\xc3\xbc" "fung fehlgeschlagen (offline?)"), "a failed check");
        UpdateChecker::Result news; news.ok = true; news.outdated = true; news.latest = "0.2.0";
        de.onResult (news);
        ok (de.download.getButtonText() == u8 ("\xe2\x86\x91 Update verf\xc3\xbc" "gbar: v0.2.0  \xe2\x80\x94  Herunterladen"), "the update line");
        auto custom = cfg;
        custom.updateNote = "Our own words.";
        VersionBadge::Panel own (badge, custom, "VST3", nullptr);
        ok (own.note.getText() == "Our own words.", "a product's own note is shown as written");
        i18n::setLanguage ("en");
    }

    //==========================================================================
    Product product;
    const auto paidConfig = [&] (int variant = 0)
    {
        auto cfg = configOf<VersionBadge::Config> (variant);
        cfg.purchase = product.purchase();
        return cfg;
    };
    std::vector<juce::String> launched;
    auto capture = [&] (VersionBadge::Panel& p) { p.launch = [&] (const juce::URL& u) { launched.push_back (u.toString (true)); return true; }; };

    group ("a paid product's window: the licence block in Feed the Cat's place");
    {
        Checker chk ("v0.1.0");
        VersionBadge badge (chk, paidConfig(), "VST3");
        const auto freeCfg = configOf<VersionBadge::Config> (0);
        VersionBadge::Panel freeP (badge, freeCfg, "VST3", nullptr);
        VersionBadge::Panel p (badge, paidConfig(), "VST3", nullptr);
        capture (p);
        ok (p.licenceBlock != nullptr && p.licenceBlock->getParentComponent() == &p && p.licenceBlock->isVisible(), "the block is there");
        ok (p.feed.getParentComponent() == nullptr && p.feedPrompt.getParentComponent() == nullptr && p.pawBtn.getParentComponent() == nullptr,
            "Feed the Cat is not");
        ok (freeP.licenceBlock == nullptr && freeP.enterKeyPage == nullptr && freeP.feed.getParentComponent() == &freeP,
            "...and a free product's window has no block, only the cat");

        // The rest of the window is the free one's: the same children above the block, in the same order.
        std::vector<juce::Component*> freeKids, paidKids;
        for (auto* c : freeP.getChildren()) if (c != &freeP.feed && c != &freeP.feedPrompt && c != &freeP.pawBtn) freeKids.push_back (c);
        for (auto* c : p.getChildren())
            if (c != p.licenceBlock.get() && c != p.enterKeyPage.get() && c != p.freeLicencePage.get()) paidKids.push_back (c);
        bool same = freeKids.size() == paidKids.size();
        for (size_t i = 0; same && i < freeKids.size(); ++i)
        {
            const juce::Component& f = *freeKids[i];
            const juce::Component& q = *paidKids[i];
            same = typeid (f) == typeid (q) && f.getBounds().getY() == q.getBounds().getY();
        }
        ok (same, "every other child is the free window's, at the same height");
        ok (p.getHeight() == freeP.getHeight() - VersionBadge::Panel::kFeedRowH + p.licenceBlock->heightFor (p.getWidth() - 28),
            "the window is the free one, less the feed row, plus the block's measured height");
        ok (p.licenceBlock->getY() >= p.result.getBottom() && p.licenceBlock->getBottom() <= p.statusArea.getY(),
            "the block sits between the update block and the address line");

        auto block = [&] (lic::Standing s)
        {
            product.now = std::move (s);
            p.refreshLicence();
            return p.licenceBlock.get();
        };

        auto* b = block ({ { lic::Status::State::trial, 23 }, std::nullopt });
        ok (b->heading.getText() == "Licence" && b->state.getText() == u8 ("Trial \xc2\xb7 23 days left"), "trial: 23 days left");
        ok (b->sentence.getText() == "Everything works. After the trial it keeps working too." && b->details.getText().isEmpty(), "trial: the sentence");
        ok (b->buy.isVisible() && b->buy.getButtonText() == "Buy Orbit480" && b->enterKey.isVisible() && b->enterKey.getButtonText() == "Enter key"
                && b->freeLink.isVisible() && b->freeLink.getButtonText() == "Free licence for students and musicians",
            "trial: Buy, Enter key and the free licence");
        b->buy.onClick();
        ok (! launched.empty() && launched.back() == "https://example.invalid/buy?product=orbit480&ref=about", "Buy opens the shop page, query kept");
        ok (p.urlUnder (&b->buy) == "https://example.invalid/buy?product=orbit480&ref=about", "...and the foot line says where it goes");
        ok (block ({ { lic::Status::State::trial, 1 }, std::nullopt })->state.getText() == u8 ("Trial \xc2\xb7 1 day left"), "trial: 1 day left");

        b = block ({ { lic::Status::State::demo, 0 }, std::nullopt });
        ok (b->state.getText() == "Not licensed" && b->sentence.getText().startsWith ("The trial is over.") && b->buy.isVisible(),
            "demo: not licensed, nothing switched off, Buy");

        b = block ({ { lic::Status::State::licensed, 0 }, ivan() });
        ok (b->state.getText() == "Licensed to Ivan Petrov", "licensed: the name");
        ok (b->details.getText() == u8 ("ivan@example.com \xc2\xb7 key 0042 \xc2\xb7 2026-11-20"), "licensed: e-mail, key number, purchase date");
        ok (b->sentence.getText() == "Thank you." && ! b->buy.isVisible() && ! b->enterKey.isVisible() && ! b->freeLink.isVisible(),
            "licensed: thank you, and nothing to buy");
        auto nameless = ivan(); nameless.name.clear();
        b = block ({ { lic::Status::State::licensed, 0 }, nameless });
        ok (b->state.getText() == "Licensed to ivan@example.com" && b->details.getText() == u8 ("key 0042 \xc2\xb7 2026-11-20"),
            "a nameless key: the e-mail is the holder, and not said twice");
        auto anonymous = nameless; anonymous.email.clear();
        ok (block ({ { lic::Status::State::licensed, 0 }, anonymous })->state.getText() == "Licensed", "neither: just licensed");

        b = block ({ { lic::Status::State::updatesOver, 0 }, ivan() });
        ok (b->state.getText() == "Licensed to Ivan Petrov" && b->details.getText() == "Updates ended 2027-11-20", "updates over: when");
        ok (b->sentence.getText().startsWith ("This version came out after your year of updates.") && b->buy.getButtonText() == "Renew at 50 % off",
            "updates over: the renewal at half price");
        b->buy.onClick();
        ok (launched.back() == "https://example.invalid/renew?product=orbit480", "Renew opens the renewal page");

        // A standing that changes height moves the window, never overlaps it.
        const int hLicensed = block ({ { lic::Status::State::licensed, 0 }, ivan() })->getHeight();
        const int hTrial    = block ({ { lic::Status::State::trial, 5 }, std::nullopt })->getHeight();
        ok (hLicensed < hTrial && p.getHeight() == freeP.getHeight() - VersionBadge::Panel::kFeedRowH + hTrial,
            "the window follows the block's height");
    }

    group ("pages: Enter key and the free licence open in the same window, Back returns");
    {
        Checker chk ("v0.1.0");
        product.now = { { lic::Status::State::trial, 23 }, std::nullopt };
        VersionBadge badge (chk, paidConfig(), "VST3");
        VersionBadge::Panel p (badge, paidConfig(), "VST3", nullptr);
        capture (p);
        const int mainH = p.getHeight();
        ok (p.page == VersionBadge::Page::main && ! p.enterKeyPage->isVisible() && ! p.freeLicencePage->isVisible(), "opens on the main page");

        p.licenceBlock->enterKey.onClick();
        ok (p.page == VersionBadge::Page::enterKey && p.enterKeyPage->isVisible() && ! p.freeLicencePage->isVisible(), "Enter key opens its page");
        ok (p.getChildren().getLast() == p.enterKeyPage.get() && p.enterKeyPage->getBounds().contains (p.stampArea)
                && p.enterKeyPage->getBounds().contains (p.licenceBlock->getBounds()) && p.enterKeyPage->getY() >= p.titleArea.getBottom(),
            "the page lies over everything under the title, on top");
        ok (p.enterKeyPage->getHeight() >= p.enterKeyPage->heightFor (p.enterKeyPage->getWidth()), "the page has the height it measured");
        {
            // Round the page, the window is only its ground: the table's frame must not show past the page's edge.
            p.asDialog = true;
            const auto img = render (p);
            int stray = 0;
            for (int y = VersionBadge::Panel::kPageTop - 2; y < p.getHeight() - 12; ++y)
                for (int x : { 3, 4, 5, 6, 7, p.getWidth() - 8, p.getWidth() - 7, p.getWidth() - 6, p.getWidth() - 5, p.getWidth() - 4 })
                    if (img.getPixelAt (x, y) != p.config.ground) ++stray;
            ok (stray == 0, "nothing of the main page shows round the page (" + std::to_string (stray) + " stray pixels)");
            p.asDialog = false;
        }
        p.enterKeyPage->top.back.onClick();
        ok (p.page == VersionBadge::Page::main && ! p.enterKeyPage->isVisible() && p.getHeight() == mainH, "Back: the main page, its own height");

        p.licenceBlock->freeLink.onClick();
        ok (p.page == VersionBadge::Page::freeLicence && p.freeLicencePage->isVisible() && ! p.enterKeyPage->isVisible(), "the free licence link opens its page");
        ok (p.freeLicencePage->getHeight() >= p.freeLicencePage->heightFor (p.freeLicencePage->getWidth())
                && p.getHeight() >= mainH, "the window grows to the page when the page needs it");
        p.freeLicencePage->top.back.onClick();
        ok (p.page == VersionBadge::Page::main && ! p.freeLicencePage->isVisible() && p.getHeight() == mainH, "Back again");

        p.showPage (VersionBadge::Page::main);

        // showAbout opens straight on a page, stays centred as the page changes the height, and a free product
        // ignores the request.
        juce::Component host;
        host.setSize (1200, 1000);
        host.addAndMakeVisible (badge);
        badge.showAbout (&host, VersionBadge::Page::enterKey);
        ok (badge.about != nullptr && badge.about->panel->page == VersionBadge::Page::enterKey && badge.about->panel->enterKeyPage->isVisible(),
            "showAbout (…, enterKey) opens on the key page");
        auto centred = [&] { return badge.about->panel->getBounds().getCentre().getDistanceFrom (host.getLocalBounds().getCentre()) <= 1; };
        ok (centred(), "...centred");
        badge.about->panel->showPage (VersionBadge::Page::freeLicence);
        ok (centred(), "still centred on another page's height");
        badge.about->panel->showPage (VersionBadge::Page::main);
        ok (centred(), "...and back");
        badge.about.reset();

        VersionBadge freeBadge (chk, configOf<VersionBadge::Config> (0), "VST3");
        host.addAndMakeVisible (freeBadge);
        freeBadge.showAbout (&host, VersionBadge::Page::enterKey);
        ok (freeBadge.about != nullptr && freeBadge.about->panel->page == VersionBadge::Page::main
                && freeBadge.about->panel->enterKeyPage == nullptr, "a free product opens on its only page");
        freeBadge.about.reset();
    }

    group ("Enter key: the product's function gets the paste, the page says its verdict");
    {
        Checker chk ("v0.1.0");
        product.now = { { lic::Status::State::trial, 23 }, std::nullopt };
        product.pasted.clear();
        VersionBadge badge (chk, paidConfig(), "VST3");
        VersionBadge::Panel p (badge, paidConfig(), "VST3", nullptr);
        capture (p);
        p.showPage (VersionBadge::Page::enterKey);
        auto& page = *p.enterKeyPage;

        const auto valid = *lic::issue (ivan(), kTestSeed);
        auto amp = ivan(); amp.product = "orbit-amp";
        const auto other = *lic::issue (amp, kTestSeed);
        auto bytes = *lic::payload (ivan());
        bytes[3] = 2;   // the format byte: a key from the future, signed by us
        const auto newer = lic::text (lic::sign (bytes, kTestSeed), "orbit480");
        auto damaged = valid;
        const auto at = damaged.find ('\n') + 7;
        damaged[at] = damaged[at] == 'A' ? 'B' : 'A';

        auto activate = [&] (const std::string& text)
        {
            page.box.setText (juce::String::fromUTF8 (text.c_str()), false);
            page.activate.onClick();
            return page.answer.getText();
        };

        ok (activate (other) == "This key is for orbit-amp, not Orbit480.", "another product's key names that product");
        ok (activate (newer) == "This key needs a newer version of Orbit480.", "a newer format asks for a newer version");
        ok (activate (damaged) == "This key is damaged: paste the whole block again.", "one changed character: damaged");
        ok (activate ("") == "This key is damaged: paste the whole block again.", "an empty box: damaged");
        ok (product.now.status.state == lic::Status::State::trial, "none of those licensed anything");

        const std::string letter = "Hi Ivan,\n\nthanks! Your key:\n\n" + valid + "\nCheers\n";
        ok (activate (letter) == "Licensed to Ivan Petrov. Thank you.", "the whole letter pasted: licensed, thank you");
        ok (! product.pasted.empty() && product.pasted.back() == juce::String::fromUTF8 (letter.c_str()),
            "the product got exactly what was pasted");
        ok (product.pasted.size() == 5, "one call per Activate (" + std::to_string (product.pasted.size()) + ")");
        ok (p.licenceBlock->state.getText() == "Licensed to Ivan Petrov", "the block already knows");
        page.top.back.onClick();
        ok (p.page == VersionBadge::Page::main && p.licenceBlock->state.getText() == "Licensed to Ivan Petrov"
                && ! p.licenceBlock->buy.isVisible(), "Back: the main page says licensed");

        // A dropped file is pasted and activated at once; only a .txt is a key file.
        product.now = { { lic::Status::State::trial, 23 }, std::nullopt };
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("appkit-about-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        dir.createDirectory();
        const auto keyFile = dir.getChildFile ("Orbit480 key.TXT");
        keyFile.replaceWithText (juce::String::fromUTF8 (valid.c_str()));
        p.showPage (VersionBadge::Page::enterKey);
        ok (page.isInterestedInFileDrag ({ keyFile.getFullPathName() }) && ! page.isInterestedInFileDrag ({ "/tmp/loop.wav" }),
            "a .txt is a key file, a .wav is not");
        const auto calls = product.pasted.size();
        page.filesDropped ({ "/tmp/loop.wav" }, 0, 0);
        ok (product.pasted.size() == calls, "a dropped .wav asks nothing");
        page.filesDropped ({ "/tmp/loop.wav", keyFile.getFullPathName() }, 0, 0);
        ok (product.pasted.size() == calls + 1, "the dropped key is handed over once");
        ok (page.box.getText() == keyFile.loadFileAsString() && product.pasted.back() == keyFile.loadFileAsString(),
            "...as the file says it, in the box too");
        ok (page.answer.getText() == "Licensed to Ivan Petrov. Thank you.", "...and it is licensed");
        dir.deleteRecursively();

        // Once the badge is gone, the product's state may be too: nothing is asked.
        auto gone = std::make_unique<VersionBadge> (chk, paidConfig(), "VST3");
        VersionBadge::Panel orphan (*gone, paidConfig(), "VST3", nullptr);
        gone.reset();
        const auto before = product.pasted.size();
        const int standingBefore = product.standingCalls;
        orphan.showPage (VersionBadge::Page::enterKey);
        orphan.enterKeyPage->box.setText ("anything");
        orphan.enterKeyPage->activate.onClick();
        orphan.showPage (VersionBadge::Page::main);
        ok (product.pasted.size() == before && product.standingCalls == standingBefore && orphan.enterKeyPage->answer.getText().isEmpty(),
            "a dead badge: neither function is called");
    }

    group ("the free licence: the letter, encoded, decodes back to the fields");
    {
        const auto subject = u8 ("Z\xc3\xb6\xc3\xab & ? # + % = / ; \xd0\x9a\xd1\x96\xd1\x82 \xe2\x9c\x93");
        const auto body = u8 ("one\ntwo\r\nthree\rfour & five? #six +seven 100% \xf0\x9f\x90\xbe");
        const auto url = lic::mailtoUrl ("licences+free@example.invalid", subject, body);
        const auto m = decodeMailto (url);
        ok (m.shaped && m.clean, "mailto:<to>?subject=…&body=…, every reserved character encoded");
        ok (m.to == "licences+free@example.invalid", "the address");
        ok (juce::String::fromUTF8 (m.subject.c_str()) == subject, "the subject, byte for byte");
        ok (juce::String::fromUTF8 (m.body.c_str()) == u8 ("one\r\ntwo\r\nthree\r\nfour & five? #six +seven 100% \xf0\x9f\x90\xbe"),
            "the body, every line break as CRLF");
        ok (! url.contains (" ") && ! url.contains ("+seven") && url.contains ("%2Bseven") && url.contains ("%0D%0A"),
            "no raw space, a '+' is never left to be read as one");

        Checker chk ("v0.1.0");
        VersionBadge badge (chk, paidConfig(), "VST3");
        VersionBadge::Panel p (badge, paidConfig(), "VST3", nullptr);
        capture (p);
        p.showPage (VersionBadge::Page::freeLicence);
        auto& page = *p.freeLicencePage;
        ok (page.choices[0].getButtonText() == "a student" && page.choices[2].getButtonText() == "someone who posted a link to Orbit480",
            "the four choices, the product named");
        ok (! page.write.isEnabled(), "nothing filled: no letter yet");
        auto settle = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (30); };   // a text change is posted
        page.name.setText (u8 ("Zo\xc3\xab O'Brien & Co"));
        page.email.setText ("zoe+test@example.com");
        settle();
        ok (! page.write.isEnabled(), "no choice yet: still no letter");
        page.choices[1].setToggleState (true, juce::sendNotification);
        ok (page.write.isEnabled() && page.choice() == 1, "a choice, a name and an e-mail: the letter can be written");
        page.choices[3].setToggleState (true, juce::sendNotification);
        ok (page.choice() == 3 && ! page.choices[1].getToggleState(), "the choices are one radio group");
        page.choices[1].setToggleState (true, juce::sendNotification);
        page.link.setText ("https://example.com/a?b=c&d=e#f");
        page.words.setText ("Line one\nLine two: 100% sure?");
        launched.clear();
        page.write.onClick();
        ok (launched.size() == 1, "Write the e-mail opens one address");
        const auto mail = decodeMailto (launched.empty() ? juce::String() : launched.back());
        ok (mail.shaped && mail.clean && mail.to == "licences@example.invalid", "to the product's address");
        ok (mail.subject == "Free licence for Orbit480", "the subject names the product");
        ok (juce::String::fromUTF8 (mail.body.c_str())
                == u8 ("I am: a musician short on money\r\nName: Zo\xc3\xab O'Brien & Co\r\nE-mail: zoe+test@example.com\r\n"
                       "Link: https://example.com/a?b=c&d=e#f\r\n\r\nLine one\r\nLine two: 100% sure?\r\n"),
            "the body is the fields, in order, with nothing lost");
        page.name.setText ("   ");
        settle();
        ok (! page.write.isEnabled(), "a blank name is no name");

        i18n::setLanguage ("de");
        const auto de = lic::freeLicenceLetter ("Orbit480", 0, "Jan", "jan@example.com", {}, {});
        ok (de.subject == u8 ("Kostenlose Lizenz f\xc3\xbcr Orbit480") && de.body.startsWith ("Das trifft auf mich zu: Ich studiere\n")
                && ! de.body.contains ("Link"), "in German, and an empty link is left out");
        i18n::setLanguage ("en");
    }

    group ("the after-trial card: Not now counts 5 4 3 2 1, restarts when shown again");
    {
        int buys = 0, keys = 0, asks = 0, notNows = 0;
        LicenceNotice::Config c;
        c.productName = "Orbit480";
        c.price = u8 ("\xe2\x82\xac" "49");
        c.onBuy = [&] { ++buys; };
        c.onEnterKey = [&] { ++keys; };
        c.onAskFree = [&] { ++asks; };
        c.onNotNow = [&] { ++notNows; };
        juce::Component parent;
        parent.setSize (900, 700);
        parent.setVisible (true);   // a window's editor is visible; the card counts only while everything above it is
        LicenceNotice card (c);
        parent.addAndMakeVisible (card);
        ok (card.isTimerRunning() && card.getTimerInterval() == 1000, "shown: the countdown's timer runs, once a second");

        ok (card.title.getText() == "Orbit480 keeps playing" && card.buy.getButtonText() == u8 ("Buy Orbit480 \xc2\xb7 \xe2\x82\xac" "49"),
            "the title and the price");
        ok (card.sentence.getText().contains ("Nothing is switched off") && card.paths[2].getText() == "you post a link to Orbit480 anywhere and tell us where"
                && card.cantPay.getText() == u8 ("Can\xe2\x80\x99t pay? We send a free licence when"), "what it says");
        ok (card.countLeft() == 5 && ! card.notNow.isEnabled() && card.notNow.getButtonText() == u8 ("Not now \xc2\xb7 5"), "shown: 5, and not pressable");
        card.notNow.triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        ok (notNows == 0 && card.isVisible(), "a click during the count does nothing");
        for (int k = 4; k >= 1; --k)
        {
            card.timerCallback();
            ok (card.countLeft() == k && ! card.notNow.isEnabled() && card.notNow.getButtonText() == u8 ("Not now \xc2\xb7 ") + juce::String (k),
                "a second later: " + std::to_string (k));
        }
        card.timerCallback();
        ok (card.countLeft() == 0 && card.notNow.isEnabled() && card.notNow.getButtonText() == "Not now", "at 0: Not now");
        ok (! card.isTimerRunning(), "...and the timer has stopped: five ticks, no more");
        card.timerCallback();
        ok (card.countLeft() == 0 && card.notNow.isEnabled(), "...and it stays there");
        card.notNow.triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        ok (notNows == 1 && ! card.isVisible(), "pressed: the card hides and tells the product");

        card.setVisible (true);
        ok (card.countLeft() == 5 && ! card.notNow.isEnabled(), "shown again: the count starts over");
        card.timerCallback(); card.timerCallback();
        card.setVisible (false);
        card.setVisible (true);
        ok (card.countLeft() == 5 && card.isTimerRunning(), "hidden mid-count and shown: from 5 again");

        // Only an ancestor hidden and shown (the product hides the area the card sits in): JUCE tells the card
        // nothing itself, and it still starts over.
        juce::Component grand, mid;
        grand.setSize (900, 700); mid.setSize (900, 700);
        grand.setVisible (true);
        grand.addAndMakeVisible (mid);
        LicenceNotice nested (cardConfig ("49"));
        mid.addAndMakeVisible (nested);
        nested.timerCallback(); nested.timerCallback(); nested.timerCallback();
        ok (nested.countLeft() == 2, "a nested card counts down");
        mid.setVisible (false);
        ok (! nested.isTimerRunning(), "its parent hidden: the count stops");
        mid.setVisible (true);
        ok (nested.countLeft() == 5 && ! nested.notNow.isEnabled() && nested.isTimerRunning(), "its parent shown again: from 5");
        for (int k = 0; k < 5; ++k) nested.timerCallback();
        grand.setVisible (false);
        grand.setVisible (true);
        ok (nested.countLeft() == 5 && ! nested.notNow.isEnabled(), "the grandparent hidden and shown, after the count ended: from 5 again");
        juce::Component other;
        other.setVisible (true);
        other.addAndMakeVisible (nested);   // moved under another parent
        mid.setVisible (false);
        ok (nested.isTimerRunning(), "moved away: the old parent's visibility is no longer its business");

        card.buy.onClick(); card.enterKey.onClick(); card.askFree.onClick();
        ok (buys == 1 && keys == 1 && asks == 1, "Buy, Enter key and Ask each call the product");

        ok (card.getWidth() == LicenceNotice::kWidth && card.getHeight() == card.heightFor (LicenceNotice::kWidth), "it measured itself");
        bool inside = true, apart = true;
        const auto kids = card.getChildren();
        for (int i = 0; i < kids.size(); ++i)
        {
            inside = inside && card.getLocalBounds().contains (kids[i]->getBounds()) && ! kids[i]->getBounds().isEmpty();
            for (int j = i + 1; j < kids.size(); ++j)
                apart = apart && ! kids[i]->getBounds().intersects (kids[j]->getBounds());
        }
        ok (inside && apart, "every piece inside the card, none overlapping");

        LicenceNotice noPrice (cardConfig ({}));
        ok (noPrice.buy.getButtonText() == "Buy Orbit480", "no price: just Buy");
    }

    group ("a paid product opens as the About dialog (a call-out closes when the app loses focus); a free one as a call-out");
    {
        Checker chk ("v0.1.0");
        juce::Component host;
        host.setSize (1200, 1000);
        host.setVisible (true);
        auto callOuts = [&]
        {
            int n = 0;
            for (auto* c : host.getChildren()) if (dynamic_cast<juce::CallOutBox*> (c) != nullptr) ++n;
            return n;
        };
        VersionBadge paid (chk, paidConfig(), "VST3");
        host.addAndMakeVisible (paid);
        paid.setBounds (1000, 950, 120, 40);
        paid.mouseUp (clickOn (paid));
        ok (paid.about != nullptr && callOuts() == 0, "a click on a paid product's badge: the About dialog, no call-out");
        paid.about.reset();
        paid.showPopup (VersionBadge::Page::freeLicence);
        ok (paid.about != nullptr && paid.about->panel->page == VersionBadge::Page::freeLicence && callOuts() == 0,
            "showPopup on a page: the About dialog, on that page");
        paid.about.reset();

        VersionBadge free (chk, configOf<VersionBadge::Config> (0), "VST3");
        host.addAndMakeVisible (free);
        free.setBounds (10, 950, 120, 40);
        free.mouseUp (clickOn (free));
        ok (free.about == nullptr && callOuts() == 1, "a click on a free product's badge: the call-out, as before");
        juce::ModalComponentManager::getInstance()->cancelAllModalComponents();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }

    group ("while a page is up, the main page's controls are hidden (no focus, no keys behind it); Back gives them back");
    {
        Checker chk ("v0.1.0");
        product.now = { { lic::Status::State::trial, 23 }, std::nullopt };
        VersionBadge badge (chk, paidConfig(), "VST3");
        VersionBadge::Panel p (badge, paidConfig(), "VST3", nullptr);
        capture (p);
        std::vector<std::pair<juce::Component*, bool>> before;
        for (auto* c : p.getChildren()) before.emplace_back (c, c->isVisible());
        p.showPage (VersionBadge::Page::enterKey);
        int behind = 0;
        for (auto* c : p.getChildren())
            if (c != p.enterKeyPage.get() && c != &p.closeBtn && c != &p.productHit && c != &p.link && c->isVisible()) ++behind;
        ok (behind == 0, "on the key page nothing of the main page is showing (" + std::to_string (behind) + " still were)");
        ok (! p.check.isVisible() && ! p.autoCheck.isVisible() && ! p.licenceBlock->isVisible(),
            "the update button, its switch and the licence block among them");
        UpdateChecker::Result news; news.ok = true; news.outdated = true; news.latest = "0.2.0";
        p.onResult (news);
        ok (! p.download.isVisible(), "an update found meanwhile stays under the page");
        p.showPage (VersionBadge::Page::freeLicence);
        ok (! p.check.isVisible(), "page to page: still hidden");
        p.showPage (VersionBadge::Page::main);
        bool same = true;
        for (auto& [c, v] : before)
            if (c != &p.download && c != &p.result && c != p.enterKeyPage.get() && c != p.freeLicencePage.get()) same = same && c->isVisible() == v;
        ok (same, "Back: every control as it was");
        ok (p.download.isVisible() && ! p.result.isVisible(), "...and the update found meanwhile shows");

        // Escape in a page's box goes back (the message is posted, so the loop runs once).
        p.showPage (VersionBadge::Page::enterKey);
        p.enterKeyPage->box.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
        ok (p.page == VersionBadge::Page::main, "Escape in the key box: back to the main page");
        p.showPage (VersionBadge::Page::freeLicence);
        p.freeLicencePage->words.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
        ok (p.page == VersionBadge::Page::main, "Escape in the letter's words: back to the main page");

        // The dialog: a click on the scrim keeps typed text; Escape goes back from a page and closes from the main one.
        juce::Component host;
        host.setSize (1200, 1000);
        host.addAndMakeVisible (badge);
        badge.showAbout (&host, VersionBadge::Page::enterKey);
        auto& about = *badge.about;
        int dismissed = 0;
        about.onDismiss = [&] { ++dismissed; };
        ok (about.panel->enterKeyPage->focusPending, "opened straight on the key page off screen: the box takes the keyboard once it shows");
        about.mouseDown (clickOn (about));
        ok (dismissed == 1, "an empty key page: a click beside the window closes it");
        about.panel->enterKeyPage->box.setText ("-----BEGIN ORBIT480 KEY-----\nIRXU");
        about.mouseDown (clickOn (about));
        ok (dismissed == 1, "a pasted key: a click beside the window does not throw it away");
        about.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
        ok (dismissed == 1 && about.panel->page == VersionBadge::Page::main, "Escape on a page: back, not out");
        about.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
        ok (dismissed == 2, "Escape on the main page: closed");
        about.panel->showPage (VersionBadge::Page::freeLicence);
        about.panel->freeLicencePage->name.setText ("Zo");
        about.mouseDown (clickOn (about));
        ok (dismissed == 2, "a letter begun: a click beside the window does not throw it away");
        badge.about.reset();
    }

    group ("the key page: a product that closes the window from its key function; the hint names the product's id");
    {
        Checker chk ("v0.1.0");
        product.now = { { lic::Status::State::trial, 23 }, std::nullopt };
        VersionBadge badge (chk, paidConfig(), "VST3");
        std::unique_ptr<VersionBadge::Panel> holder;
        auto cfg = paidConfig();
        cfg.purchase->enterKey = [&] (const juce::String& s)
        {
            holder.reset();   // the product closes About once licensed — the page that asked is gone when this returns
            return lic::read (s.toStdString(), product.pub, "orbit480");
        };
        holder = std::make_unique<VersionBadge::Panel> (badge, cfg, "VST3", nullptr);
        holder->showPage (VersionBadge::Page::enterKey);
        auto* page = holder->enterKeyPage.get();
        page->box.setText (juce::String (*lic::issue (ivan(), kTestSeed)));
        page->activateNow();   // must not touch the page after the call (ASan names the read when it does)
        ok (holder == nullptr, "the window went away inside the call, and the page did not touch itself after");

        auto ampCfg = paidConfig();
        ampCfg.productName = "Orbit Amp";
        ampCfg.purchase->productId = "orbit-amp";
        VersionBadge::Panel amp (badge, ampCfg, "VST3", nullptr);
        ok (amp.enterKeyPage->box.getTextToShowWhenEmpty() == "-----BEGIN ORBIT-AMP KEY-----", "the hint is the id's BEGIN line, as in the letter");
    }

    group ("an empty shop or renewal address hides its button");
    {
        Checker chk ("v0.1.0");
        VersionBadge badge (chk, paidConfig(), "VST3");
        auto cfg = paidConfig();
        cfg.purchase->buyUrl = {};
        cfg.purchase->renewUrl = {};
        product.now = { { lic::Status::State::trial, 9 }, std::nullopt };
        VersionBadge::Panel p (badge, cfg, "VST3", nullptr);
        ok (! p.licenceBlock->buy.isVisible() && p.licenceBlock->enterKey.isVisible()
                && p.licenceBlock->enterKey.getX() == p.licenceBlock->kPad, "trial, no shop address: no Buy; Enter key takes the first place");
        product.now = { { lic::Status::State::updatesOver, 0 }, ivan() };
        p.refreshLicence();
        ok (! p.licenceBlock->buy.isVisible() && p.licenceBlock->enterKey.isVisible(), "updates over, no renewal address: no Renew");
        ok (layoutFaults (*p.licenceBlock).empty(), "...and the block lays out clean: " + joined (layoutFaults (*p.licenceBlock)));
    }

    group ("the free licence: a few words are a few, the letter's address stays under 1800 characters, the address is on the page");
    {
        Checker chk ("v0.1.0");
        VersionBadge badge (chk, paidConfig(), "VST3");
        VersionBadge::Panel p (badge, paidConfig(), "VST3", nullptr);
        capture (p);
        p.showPage (VersionBadge::Page::freeLicence);
        auto& page = *p.freeLicencePage;
        page.words.insertTextAtCaret (juce::String::repeatedString ("word ", 120));
        ok (page.words.getText().length() == lic::kMaxLetterWords, "the words box takes 300 characters and no more");
        const auto letter = lic::freeLicenceLetter ("Orbit480", 0, "A", "a@b.c", {}, juce::String::repeatedString ("x", 500));
        ok (letter.body.endsWith ("\n" + juce::String::repeatedString ("x", 300) + "\n"), "the letter keeps the first 300 characters of them");

        const auto cyr = u8 ("\xd0\x96");   // two bytes, six characters encoded
        const auto allWords = juce::String::repeatedString (cyr, lic::kMaxLetterWords);
        page.choices[0].setToggleState (true, juce::sendNotification);
        page.name.setText ("Zoe");
        page.email.setText ("someone.with.a.long.address@example.invalid");
        page.words.setText (allWords);
        {
            // Only the words are long: they are cut, from the end, until the address fits.
            const auto url = page.url();
            const auto m = decodeMailto (url);
            const auto body = juce::String::fromUTF8 (m.body.c_str());
            const auto words = body.fromFirstOccurrenceOf ("\r\n\r\n", false, false).trimEnd();
            ok (url.length() <= lic::FreeLicencePage::kMaxMailto && m.shaped && m.clean,
                "300 Cyrillic words: the address is " + std::to_string (url.length()) + " characters, at most 1800, and decodes");
            ok (words.isNotEmpty() && words.length() < allWords.length() && allWords.startsWith (words),
                "...what is left of the words is their beginning (" + std::to_string (words.length()) + " of 300)");
            ok (body.contains ("Name: Zoe\r\n") && body.contains ("E-mail: someone.with.a.long.address@example.invalid"), "...name and e-mail whole");
        }
        page.name.setText (juce::String::repeatedString (cyr, lic::FreeLicencePage::kMaxName));
        page.link.setText (juce::String::repeatedString (cyr, lic::FreeLicencePage::kMaxLink));
        {
            // Everything at its longest: the words go, then the link, then the name; the e-mail never.
            const auto url = page.url();
            const auto m = decodeMailto (url);
            ok (url.length() <= lic::FreeLicencePage::kMaxMailto, "every field at its longest: " + std::to_string (url.length()) + " characters, at most 1800");
            ok (m.shaped && m.clean && juce::String::fromUTF8 (m.body.c_str()).contains ("E-mail: someone.with.a.long.address@example.invalid"),
                "...and still decodes, with the e-mail whole");
        }

        ok (page.addressBox.getText() == "licences@example.invalid" && page.addressBox.isReadOnly() && page.addressBox.isVisible(),
            "the address is on the page, selectable, read-only");
        juce::String copied;
        page.copyText = [&] (const juce::String& s) { copied = s; };
        page.copy.onClick();
        ok (copied == "licences@example.invalid" && page.copy.getButtonText() == u8 ("copied \xe2\x9c\x93"), "Copy puts the address on the clipboard and says so");

        page.name.setText ("Zoe");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
        p.launch = [] (const juce::URL&) { return false; };
        page.write.onClick();
        ok (page.failed.isVisible() && page.failed.getText() == u8 ("No mail app opened \xe2\x80\x94 write to licences@example.invalid"),
            "no mail app opened: the page says where to write");
        p.launch = [] (const juce::URL&) { return true; };
        page.write.onClick();
        ok (! page.failed.isVisible(), "a mail app opened: the line goes");
        ok (layoutFaults (page).empty(), "the page lays out clean: " + joined (layoutFaults (page)));
    }

    group ("a product's own wording is shown as written, even when it happens to be another sentence the window knows");
    {
        Checker chk ("v0.1.0");
        auto cfg = configOf<VersionBadge::Config> (0);
        cfg.feedPrompt = "Close";
        cfg.updateNote = "Check for updates";
        cfg.updateNoteDetail = "Back";
        VersionBadge badge (chk, cfg, "VST3");
        i18n::setLanguage ("de");
        VersionBadge::Panel p (badge, cfg, "VST3", nullptr);
        ok (p.feedPrompt.getText() == "Close" && p.note.getText() == "Check for updates" && p.note.getTooltip() == "Back",
            "in German: \"Close\", \"Check for updates\", \"Back\" — as the product wrote them");
        auto defaults = configOf<VersionBadge::Config> (0);
        VersionBadge::Panel d (badge, defaults, "VST3", nullptr);
        ok (d.feedPrompt.getText() != juce::String (VersionBadge::Config::kDefaultFeedPrompt)
                && d.note.getText() != juce::String (VersionBadge::Config::kDefaultUpdateNote), "the defaults are translated");
        i18n::setLanguage ("en");
    }

    group ("the card for a key whose updates ended: its own sentence, and the renewal");
    {
        auto c = cardConfig ("49");
        c.updatesOver = true;
        c.updatesUntil = static_cast<std::uint16_t> (lic::dayFromCivil (2027, 11, 20));
        c.renewUrl = "https://example.invalid/renew?product=orbit480";
        LicenceNotice card (c);
        std::vector<juce::String> opened;
        card.launch = [&] (const juce::URL& u) { opened.push_back (u.toString (true)); return true; };
        ok (card.sentence.getText() == "Your year of updates ended on 2027-11-20; this version came out after it. Your licence keeps every version up to 2027-11-20.",
            "the sentence names the day, twice");
        ok (card.buy.isVisible() && card.buy.getButtonText() == "Renew at 50 % off", "the first button is the renewal");
        card.buy.onClick();
        ok (opened.size() == 1 && opened[0] == "https://example.invalid/renew?product=orbit480", "...and opens the renewal page");
        ok (layoutFaults (card).empty(), "it lays out clean: " + joined (layoutFaults (card)));
        c.renewUrl = {};
        LicenceNotice noRenew (c);
        ok (! noRenew.buy.isVisible() && noRenew.enterKey.isVisible() && layoutFaults (noRenew).empty(), "no renewal address: no button");
        i18n::setLanguage ("de");
        LicenceNotice de (c);
        ok (de.sentence.getText().startsWith ("Ihr Update-Jahr endete am 2027-11-20"), "in German");
        i18n::setLanguage ("en");
    }

    group ("every screen lays out clean in all twelve languages: pieces inside, none overlapping, no words cut");
    {
        Checker chk ("v0.1.0");
        for (const auto& lang : i18n::kLanguages)
        {
            const std::string code (lang.code);
            i18n::setLanguage (juce::String (code));
            VersionBadge badge (chk, paidConfig(), "VST3");
            std::vector<std::string> faults;
            auto collect = [&] (const std::string& where, juce::Component& c)
            {
                for (const auto& f : layoutFaults (c)) faults.push_back (where + ": " + f);
            };
            for (auto s : { lic::Standing { { lic::Status::State::trial, 23 }, std::nullopt },
                            lic::Standing { { lic::Status::State::demo, 0 }, std::nullopt },
                            lic::Standing { { lic::Status::State::licensed, 0 }, ivan() },
                            lic::Standing { { lic::Status::State::updatesOver, 0 }, ivan() } })
            {
                product.now = s;
                VersionBadge::Panel p (badge, paidConfig(), "VST3", nullptr);
                collect ("block", *p.licenceBlock);
                if (s.status.state == lic::Status::State::trial)
                {
                    p.showPage (VersionBadge::Page::enterKey);
                    lic::Reading r; r.verdict = lic::Verdict::otherProduct; r.key = ivan(); r.key.product = "orbit-amp";
                    p.enterKeyPage->showAnswer (r);
                    p.showPage (VersionBadge::Page::enterKey);   // measured again with the answer
                    p.enterKeyPage->showAnswer (r);
                    collect ("key page", *p.enterKeyPage);
                    p.showPage (VersionBadge::Page::freeLicence);
                    p.freeLicencePage->failed.setVisible (true);
                    p.freeLicencePage->resized();
                    collect ("free page", *p.freeLicencePage);
                    ok (p.freeLicencePage->getBottom() <= p.getHeight() && p.freeLicencePage->getHeight() >= p.freeLicencePage->heightFor (p.freeLicencePage->getWidth()),
                        code + ": the free page fits the window");
                }
            }
            LicenceNotice demo (cardConfig ("49"));
            collect ("card", demo);
            auto over = cardConfig ({});
            over.updatesOver = true; over.updatesUntil = 21142; over.renewUrl = "https://example.invalid/renew";
            LicenceNotice renewal (over);
            collect ("renewal card", renewal);
            ok (faults.empty(), code + ": " + (faults.empty() ? std::string ("clean") : joined (faults)));
        }
        i18n::setLanguage ("en");
    }

    group ("every licence screen speaks every language: no English left in German, nothing empty anywhere");
    {
        Checker chk ("v0.1.0");
        auto screens = [&] (std::vector<juce::String>& out)
        {
            VersionBadge badge (chk, paidConfig(), "VST3");
            for (auto s : { lic::Standing { { lic::Status::State::trial, 23 }, std::nullopt },
                            lic::Standing { { lic::Status::State::demo, 0 }, std::nullopt },
                            lic::Standing { { lic::Status::State::licensed, 0 }, ivan() },
                            lic::Standing { { lic::Status::State::updatesOver, 0 }, ivan() } })
            {
                product.now = s;
                VersionBadge::Panel p (badge, paidConfig(), "VST3", nullptr);
                textsOf (*p.licenceBlock, out);
                if (s.status.state == lic::Status::State::trial)
                {
                    textsOf (*p.enterKeyPage, out);
                    textsOf (*p.freeLicencePage, out);
                    out.push_back (p.check.getButtonText());
                    out.push_back (p.note.getText());
                    for (auto v : { lic::Verdict::valid, lic::Verdict::otherProduct, lic::Verdict::newerFormat, lic::Verdict::damaged })
                    {
                        lic::Reading r; r.verdict = v; r.key = ivan();
                        p.enterKeyPage->showAnswer (r);
                        out.push_back (p.enterKeyPage->answer.getText());
                    }
                }
            }
            LicenceNotice card (cardConfig ("49"));
            textsOf (card, out);
            auto over = cardConfig ("49");
            over.updatesOver = true; over.updatesUntil = 21142; over.renewUrl = "https://example.invalid/renew";
            LicenceNotice renewal (over);
            textsOf (renewal, out);
        };
        std::vector<juce::String> en;
        screens (en);
        for (const auto& lang : i18n::kLanguages)
        {
            if (lang.code == "en") continue;
            i18n::setLanguage (juce::String (std::string (lang.code)));
            std::vector<juce::String> other;
            screens (other);
            int same = 0, empty = 0;
            for (size_t i = 0; i < en.size() && i < other.size(); ++i)
            {
                if (other[i].trim().isEmpty() && en[i].isNotEmpty()) ++empty;
                // The one text that is the same in every language: the key block's own BEGIN line.
                if (lang.code == "de" && en[i].isNotEmpty() && other[i] == en[i] && ! en[i].startsWith ("-----BEGIN"))
                {
                    ++same;
                    ok (false, "de still says \"" + en[i].toStdString() + "\"");
                }
            }
            ok (other.size() == en.size() && empty == 0,
                std::string (lang.code) + ": " + std::to_string (other.size()) + " texts, none empty");
            if (lang.code == "de") ok (same == 0, "de: every text translated (" + std::to_string (en.size()) + " texts)");
        }
        i18n::setLanguage ("en");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}

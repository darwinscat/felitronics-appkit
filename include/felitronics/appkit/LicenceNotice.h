// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

//==============================================================================
// felitronics::appkit::LicenceNotice — the card a paid product lays over its own window (its spectrum, say) once the
// trial is over and the window is open. juce_gui_basics.
//
// It says the product keeps playing — nothing is switched off — and offers what can be done: buy, enter a key, or ask
// for one of the four free licences. "Not now" counts 5 4 3 2 1 before it can be pressed (a second each, restarted
// every time the card is shown), and then hides the card; the product keeps it hidden until its window opens again.
// The card does nothing itself but hide: every button is a callback to the product, which opens its shop page or
// its About window on the right page (VersionBadge::showAbout).
//
// The card sizes itself in its constructor (width kWidth, height measured from what it says in the current
// language); the product only places it, e.g. setCentrePosition over the area it covers. The controls around it keep
// working — the card covers only itself.
//==============================================================================

#include <juce_gui_basics/juce_gui_basics.h>

#include <felitronics/appkit/I18n.h>
#include <felitronics/appkit/LicencePages.h>

#include <array>
#include <functional>
#include <utility>

namespace felitronics::appkit
{

class LicenceNotice final : public juce::Component,
                            private juce::Timer
{
public:
    struct Config
    {
        juce::String productName;   // REQUIRED
        juce::String price;         // "€49"; empty drops the " · price" from the button

        // The product's own colours, as its About window has them (VersionBadge::Config).
        juce::Colour accent      { 0xff7c4dff };
        juce::Colour accentHover { 0xff9778ff };
        juce::Colour accentB     { 0xffff8822 };
        juce::Colour text        { 0xffd8d8d8 };
        juce::Colour ground      { 0xff111119 };

        std::function<void()> onBuy;        // open the shop page
        std::function<void()> onEnterKey;   // About, on its "Enter key" page
        std::function<void()> onAskFree;    // About, on its "Ask for a free licence" page
        std::function<void()> onNotNow;     // the card has hidden itself; keep it hidden until the window reopens
    };

    static constexpr int kWidth = 600, kCountFrom = 5;

    explicit LicenceNotice (Config c) : config (std::move (c))
    {
        using i18n::fill;
        using i18n::tr;
        const auto product = [this] (const char* key) { return fill (tr (key), { { "product", config.productName } }); };
        const juce::Colour ink = config.text.withAlpha (0.75f);

        title.set (product ("{product} keeps playing"), juce::Font (juce::FontOptions (20.0f, juce::Font::bold)), config.text);
        sentence.set (product ("Your trial is over. Nothing is switched off and the sound stays the same. If {product} "
                               "has earned its place in your mixes, please buy it."),
                      juce::Font (juce::FontOptions (14.0f)), ink);
        cantPay.set (tr ("Can\xe2\x80\x99t pay? We send a free licence when"),
                     juce::Font (juce::FontOptions (13.0f, juce::Font::bold)), config.text);
        static constexpr const char* kPaths[] { "you are a student", "you are a musician short on money",
                                                "you post a link to {product} anywhere and tell us where",
                                                "you send us your music: if we like it (we will)" };
        for (size_t i = 0; i < paths.size(); ++i)
        {
            paths[i].set (product (kPaths[i]), juce::Font (juce::FontOptions (13.0f)), ink);
            addAndMakeVisible (paths[i]);
        }
        for (auto* p : { &title, &sentence, &cantPay })
            addAndMakeVisible (*p);

        buy.fill = config.accentB;
        buy.setButtonText (config.price.isNotEmpty()
                               ? fill (tr ("Buy {product} \xc2\xb7 {price}"), { { "product", config.productName }, { "price", config.price } })
                               : product ("Buy {product}"));
        buy.onClick = [this] { if (config.onBuy) config.onBuy(); };
        addAndMakeVisible (buy);

        enterKey.edge = config.accent;
        enterKey.ink  = config.text;
        enterKey.setButtonText (tr ("Enter key"));
        enterKey.onClick = [this] { if (config.onEnterKey) config.onEnterKey(); };
        addAndMakeVisible (enterKey);

        askFree.colour = config.accentHover;
        askFree.setButtonText (tr ("Ask for a free licence"));
        askFree.onClick = [this] { if (config.onAskFree) config.onAskFree(); };
        addAndMakeVisible (askFree);

        notNow.colour = config.text.withAlpha (0.7f);
        notNow.justification = juce::Justification::centredRight;
        notNow.onClick = [this] { dismiss(); };
        addAndMakeVisible (notNow);

        setSize (kWidth, heightFor (kWidth));
        restartCountdown();
    }

    /** 5 4 3 2 1 while "Not now" waits, 0 once it can be pressed. */
    int countLeft() const { return left; }

    /** Back to 5, "Not now" disabled. Called whenever the card is shown. */
    void restartCountdown()
    {
        left = kCountFrom;
        showCount();
        startTimer (1000);
    }

    int heightFor (int width) const
    {
        const int w = width - 2 * kPadX;
        const int col = (w - kColGap) / 2;
        int h = kPadY + title.heightFor (w) + 14 + sentence.heightFor (w) + 16 + licence::PillButton::kHeight + 18
              + cantPay.heightFor (w) + 8;
        for (size_t i = 0; i < paths.size(); i += 2)
            h += juce::jmax (paths[i].heightFor (col), paths[i + 1].heightFor (col)) + (i == 0 ? 8 : 0);
        return h + 16 + kFootH + kPadY;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (kPadX, kPadY);
        const int w = r.getWidth();
        const int col = (w - kColGap) / 2;
        title.setBounds (r.removeFromTop (title.heightFor (w)));
        r.removeFromTop (14);
        sentence.setBounds (r.removeFromTop (sentence.heightFor (w)));
        r.removeFromTop (16);
        {
            auto row = r.removeFromTop (licence::PillButton::kHeight);
            const int half = (row.getWidth() - 12) / 2;
            buy.setBounds (row.removeFromLeft (half));
            enterKey.setBounds (row.removeFromRight (half));
        }
        r.removeFromTop (18);
        cantPay.setBounds (r.removeFromTop (cantPay.heightFor (w)));
        r.removeFromTop (8);
        for (size_t i = 0; i < paths.size(); i += 2)
        {
            auto row = r.removeFromTop (juce::jmax (paths[i].heightFor (col), paths[i + 1].heightFor (col)));
            paths[i].setBounds (row.removeFromLeft (col));
            paths[i + 1].setBounds (row.removeFromRight (col));
            if (i == 0) r.removeFromTop (8);
        }
        r.removeFromTop (16);
        auto foot = r.removeFromTop (kFootH);
        notNow.setBounds (foot.removeFromRight (notNow.idealWidth()));
        askFree.setBounds (foot.removeFromLeft (juce::jmin (foot.getWidth(), askFree.idealWidth())));
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (config.ground.withAlpha (0.96f));
        g.fillRoundedRectangle (b, 14.0f);
        g.setColour (config.accentB.withAlpha (0.55f));
        g.drawRoundedRectangle (b, 14.0f, 1.0f);
    }

private:
    void visibilityChanged() override
    {
        if (isVisible()) restartCountdown();
        else             stopTimer();
    }

    void timerCallback() override
    {
        if (left > 0)
            --left;
        showCount();
        if (left == 0)
            stopTimer();
    }

    void showCount()
    {
        notNow.setButtonText (left > 0 ? i18n::fill (i18n::tr ("Not now \xc2\xb7 {n}"), { { "n", juce::String (left) } })
                                       : i18n::tr ("Not now"));
        notNow.setEnabled (left == 0);
        if (getWidth() > 0)
            resized();   // the count is one character the word does not have
    }

    void dismiss()
    {
        if (left > 0)
            return;
        setVisible (false);
        if (config.onNotNow) config.onNotNow();
    }

    static constexpr int kPadX = 32, kPadY = 28, kColGap = 18, kFootH = 20;

    Config                          config;
    licence::Para                   title, sentence, cantPay;
    std::array<licence::Para, 4>    paths;
    licence::PillButton             buy { true }, enterKey { false };
    licence::TextLink               askFree, notNow;
    int                             left = kCountFrom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenceNotice)
};

} // namespace felitronics::appkit

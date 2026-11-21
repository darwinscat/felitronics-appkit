// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
#pragma once

//==============================================================================
// felitronics::appkit::licence — the paying half of a paid product's About window: the licence block that takes the
// place of "Feed the Cat", the page that takes a key, the page that asks for a free licence, and the letter that page
// writes. juce_gui_basics; no Monocypher (the product reads the key, see LicenceKey.h) and no storage (the product
// keeps the key, see LicenceStore.h): the window only shows what the product says and hands it what was pasted.
//
// VersionBadge builds these when its Config carries a Purchase; LicenceNotice (the after-trial card) borrows the
// buttons. Everything they say goes through i18n::tr.
//==============================================================================

#include <juce_gui_basics/juce_gui_basics.h>

#include <felitronics/appkit/I18n.h>
#include <felitronics/appkit/LicenceTypes.h>

#include <array>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>

namespace felitronics::appkit::licence
{
/** Where the product stands now, as its About window shows it: `status` from licence::status, and the stored valid
    key for this product (licensed / updatesOver), if any. */
struct Standing
{
    Status             status;
    std::optional<Key> key;
};

/** What a paid product hands its About window (VersionBadge::Config::purchase). The functions are called on the
    message thread, and only while the badge is alive — like the update checker, whatever they capture must outlive
    the badge, never the window. */
struct Purchase
{
    std::function<Standing()> standing;                          // REQUIRED: read on every visit to the main page
    std::function<Reading (const juce::String& pasted)> enterKey; // REQUIRED: checks the paste, stores a valid key, answers
    juce::String buyUrl;          // "Buy": the product's page in the shop
    juce::String renewUrl;        // the renewal offer once a key's year of updates is over
    juce::String freeLicenceTo;   // the address the free-licence letter goes to
};

/** The colours the screens are painted with: the About window's own (VersionBadge::Config). */
struct Look
{
    juce::Colour accent      { 0xff7c4dff };
    juce::Colour accentHover { 0xff9778ff };
    juce::Colour accentB     { 0xffff8822 };
    juce::Colour text        { 0xffd8d8d8 };
    juce::Colour ground      { 0xff111119 };
    juce::Colour ink         { 0xff9a9aa4 };   // the window's plain text
};

//==============================================================================
// The letter.

/** RFC 3986 percent-encoding of the UTF-8 bytes, everything but the unreserved characters encoded; a line break
    (LF, CR or CRLF) becomes %0D%0A, as RFC 6068 wants line breaks in a mailto body. */
inline juce::String percentEncode (const juce::String& s)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    juce::String out;
    const char* p = s.toRawUTF8();
    for (; *p != 0; ++p)
    {
        const auto b = static_cast<unsigned char> (*p);
        if (b == '\r' || b == '\n')
        {
            if (b == '\r' && p[1] == '\n') ++p;
            out << "%0D%0A";
        }
        else if ((b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || (b >= '0' && b <= '9')
                 || b == '-' || b == '.' || b == '_' || b == '~')
            out << juce::String::charToString (static_cast<juce::juce_wchar> (b));
        else
            out << "%" << juce::String::charToString (hex[b >> 4]) << juce::String::charToString (hex[b & 15]);
    }
    return out;
}

/** "mailto:<to>?subject=…&body=…", every field percent-encoded (the address keeps its '@'). Opened as it is — see
    FreeLicencePage — so nothing re-reads the query and turns a '+' into a space on the way to the mail app. */
inline juce::String mailtoUrl (const juce::String& to, const juce::String& subject, const juce::String& body)
{
    return "mailto:" + percentEncode (to).replace ("%40", "@") + "?subject=" + percentEncode (subject)
         + "&body=" + percentEncode (body);
}

struct Letter
{
    juce::String subject, body;
};

/** The free-licence letter, in the reader's language: they read it in their own mail app before they send it.
    `choice` is 0..3 in the page's order (student, musician short on money, posted a link, sending music). */
inline Letter freeLicenceLetter (const juce::String& product, int choice, const juce::String& name,
                                 const juce::String& email, const juce::String& link, const juce::String& words)
{
    static constexpr const char* kChoices[] { "a student", "a musician short on money",
                                              "someone who posted a link to {product}", "sending you my music" };
    using i18n::fill;
    using i18n::tr;
    const auto field = [] (const char* label, const juce::String& value) { return tr (label) + ": " + value.trim() + "\n"; };

    Letter l;
    l.subject = fill (tr ("Free licence for {product}"), { { "product", product } });
    if (choice >= 0 && choice < 4)
        l.body << field ("I am", fill (tr (kChoices[choice]), { { "product", product } }));
    l.body << field ("Name", name) << field ("E-mail", email);
    if (link.trim().isNotEmpty())
        l.body << field ("Link", link);
    if (words.trim().isNotEmpty())
        l.body << "\n" << words.trim() << "\n";
    return l;
}

//==============================================================================
// The pieces the screens are built from.

inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

/** A paragraph that wraps to its width: measured and painted by the SAME arrangement, so the height a layout asks
    for is the height it gets — a Label fits its text its own way and clips the line the measurement did not see. */
struct Para final : public juce::Component
{
    Para() { setInterceptsMouseClicks (false, false); }

    void set (const juce::String& t, const juce::Font& f, juce::Colour c)
    {
        text = t; font = f; colour = c;
        repaint();
    }

    void setText (const juce::String& t) { text = t; repaint(); }
    void setColour (juce::Colour c)      { colour = c; repaint(); }
    const juce::String& getText() const  { return text; }

    int heightFor (int width) const
    {
        if (text.isEmpty())
            return 0;
        return (int) std::ceil (arrange ((float) width).getBoundingBox (0, -1, true).getBottom());
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (colour);
        arrange ((float) getWidth()).draw (g);
    }

    juce::GlyphArrangement arrange (float width) const
    {
        juce::GlyphArrangement ga;
        ga.addJustifiedText (font, text, 0.0f, font.getAscent(), juce::jmax (1.0f, width), juce::Justification::topLeft);
        return ga;
    }

    juce::String text;
    juce::Font   font { juce::FontOptions (13.0f) };
    juce::Colour colour { 0xff9a9aa4 };
};

/** A button with a face of its own: filled (the one the window wants pressed) or outlined. */
struct PillButton final : public juce::Button
{
    explicit PillButton (bool isFilled) : juce::Button ({}), filled (isFilled)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::Font font() const { return juce::Font (juce::FontOptions (14.0f, filled ? juce::Font::bold : juce::Font::plain)); }
    int idealWidth() const  { return (int) std::ceil (textWidth (font(), getButtonText())) + 36; }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const auto b = getLocalBounds().toFloat().reduced (0.5f);
        const float a = isEnabled() ? 1.0f : 0.4f;
        if (filled)
        {
            g.setColour (fill.withMultipliedBrightness (over && isEnabled() ? 1.08f : 1.0f).withMultipliedAlpha (a));
            g.fillRoundedRectangle (b, 8.0f);
        }
        else
        {
            if (over && isEnabled())
            {
                g.setColour (edge.withAlpha (0.10f));
                g.fillRoundedRectangle (b, 8.0f);
            }
            g.setColour (edge.withMultipliedAlpha (a));
            g.drawRoundedRectangle (b, 8.0f, 1.0f);
        }
        g.setColour (ink.withMultipliedAlpha (a));
        g.setFont (font());
        g.drawText (getButtonText(), getLocalBounds().reduced (8, 0), juce::Justification::centred, true);
    }

    void enablementChanged() override
    {
        setMouseCursor (isEnabled() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    static constexpr int kHeight = 34;

    bool         filled;
    juce::Colour fill { 0xffff8822 }, edge { 0xff7c4dff }, ink { 0xff1a0f08 };
};

/** A line of text that is a button: underlined under the pointer, dimmed and inert while disabled. */
struct TextLink final : public juce::Button
{
    TextLink() : juce::Button ({}) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    int idealWidth() const { return (int) std::ceil (textWidth (font, getButtonText())) + 4; }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto f = font;
        f.setUnderline (over && isEnabled());
        g.setFont (f);
        g.setColour (colour.withMultipliedAlpha (isEnabled() ? 1.0f : 0.5f));
        g.drawText (getButtonText(), getLocalBounds(), justification, true);
    }

    void enablementChanged() override
    {
        setMouseCursor (isEnabled() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    juce::Font          font { juce::FontOptions (13.0f) };
    juce::Colour        colour { 0xff9778ff };
    juce::Justification justification = juce::Justification::centredLeft;
};

inline void styleEditor (juce::TextEditor& e, const Look& look, const juce::Font& font)
{
    e.setFont (font);
    e.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.25f));
    e.setColour (juce::TextEditor::outlineColourId, look.accent.withAlpha (0.35f));
    e.setColour (juce::TextEditor::focusedOutlineColourId, look.accent);
    e.setColour (juce::TextEditor::textColourId, look.text);
    e.setColour (juce::TextEditor::highlightColourId, look.accent.withAlpha (0.4f));
    e.setColour (juce::CaretComponent::caretColourId, look.accentB);
    e.setIndents (8, 5);
}

//==============================================================================
/** The licence block: what the product's standing is, and what can be done about it. Sits where "Feed the Cat" sits
    in a free product's window. */
class LicenceBlock final : public juce::Component
{
public:
    LicenceBlock (Look l, juce::String product) : look (l), productName (std::move (product))
    {
        // Not upper-cased: a locale-blind toUpperCase writes Turkish "Lisans" as LISANS, which is not the word.
        heading.set (i18n::tr ("Licence"),
                     juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.08f),
                     look.text.withAlpha (0.6f));
        addAndMakeVisible (heading);
        addAndMakeVisible (state);
        addAndMakeVisible (details);
        addAndMakeVisible (sentence);

        buy.fill = look.accentB;
        addChildComponent (buy);
        enterKey.edge = look.accent;
        enterKey.ink  = look.text;
        enterKey.setButtonText (i18n::tr ("Enter key"));
        enterKey.onClick = [this] { if (onEnterKey) onEnterKey(); };
        addChildComponent (enterKey);
        freeLink.colour = look.accentHover;
        freeLink.setButtonText (i18n::tr ("Free licence for students and musicians"));
        freeLink.onClick = [this] { if (onFreeLicence) onFreeLicence(); };
        addChildComponent (freeLink);
        buy.onClick = [this] { if (launch && buyTarget.isWellFormed()) launch (buyTarget); };
    }

    /** Says what `s` is: texts, and which buttons there are. */
    void refresh (const Standing& s, const Purchase& p)
    {
        const juce::Font big (juce::FontOptions (17.0f, juce::Font::bold));
        const juce::Font body (juce::FontOptions (13.0f));
        const auto mid = juce::String::fromUTF8 (" \xc2\xb7 ");
        auto holder = [&]
        {
            if (! s.key) return i18n::tr ("Licensed");
            const auto who = juce::String::fromUTF8 (s.key->name.c_str()).trim().isNotEmpty()
                                 ? juce::String::fromUTF8 (s.key->name.c_str()).trim()
                                 : juce::String::fromUTF8 (s.key->email.c_str()).trim();
            return who.isNotEmpty() ? i18n::fill (i18n::tr ("Licensed to {name}"), { { "name", who } }) : i18n::tr ("Licensed");
        };

        juce::String stateText, detailText, sentenceText;
        bool actions = true;
        buyTarget = juce::URL (p.buyUrl);
        buy.setButtonText (i18n::fill (i18n::tr ("Buy {product}"), { { "product", productName } }));
        switch (s.status.state)
        {
            case Status::State::trial:
                stateText    = i18n::trCount ("Trial \xc2\xb7 {n} days left", s.status.trialDaysLeft);
                sentenceText = i18n::tr ("Everything works. After the trial it keeps working too.");
                break;
            case Status::State::demo:
                stateText    = i18n::tr ("Not licensed");
                sentenceText = i18n::tr ("The trial is over. Nothing is switched off and the sound stays the same.");
                break;
            case Status::State::licensed:
            {
                stateText = holder();
                juce::StringArray parts;
                if (s.key)
                {
                    if (! s.key->email.empty() && ! s.key->name.empty())   // a nameless key's e-mail is already the state line
                        parts.add (juce::String::fromUTF8 (s.key->email.c_str()));
                    parts.add (i18n::fill (i18n::tr ("key {number}"),
                                           { { "number", juce::String (s.key->number).paddedLeft ('0', 4) } }));
                    parts.add (juce::String (isoDate (s.key->issued)));
                }
                detailText   = parts.joinIntoString (mid);
                sentenceText = i18n::tr ("Thank you.");
                actions      = false;
                break;
            }
            case Status::State::updatesOver:
                stateText  = holder();
                detailText = s.key ? i18n::fill (i18n::tr ("Updates ended {date}"),
                                                 { { "date", juce::String (isoDate (s.key->updatesUntil)) } })
                                   : juce::String();
                sentenceText = i18n::tr ("This version came out after your year of updates. It keeps working as the demo.");
                buy.setButtonText (i18n::tr ("Renew at 50 % off"));
                buyTarget = juce::URL (p.renewUrl);
                break;
        }

        state.set (stateText, big, look.text);
        details.set (detailText, body, look.ink);
        sentence.set (sentenceText, body, look.ink);
        buy.setVisible (actions);
        enterKey.setVisible (actions);
        freeLink.setVisible (actions);
        resized();
        repaint();
    }

    /** The height the block needs at `width`: measured, the way it is laid out. */
    int heightFor (int width) const
    {
        const int w = width - 2 * kPad;
        int h = kPad + heading.heightFor (w) + 6 + state.heightFor (w);
        if (details.getText().isNotEmpty())  h += 4 + details.heightFor (w);
        if (sentence.getText().isNotEmpty()) h += 4 + sentence.heightFor (w);
        if (buy.isVisible())                 h += 12 + PillButton::kHeight + 8 + kLinkH;
        return h + kPad;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (kPad);
        const int w = r.getWidth();
        heading.setBounds (r.removeFromTop (heading.heightFor (w)));
        r.removeFromTop (6);
        state.setBounds (r.removeFromTop (state.heightFor (w)));
        if (details.getText().isNotEmpty())  { r.removeFromTop (4); details.setBounds (r.removeFromTop (details.heightFor (w))); }
        if (sentence.getText().isNotEmpty()) { r.removeFromTop (4); sentence.setBounds (r.removeFromTop (sentence.heightFor (w))); }
        if (buy.isVisible())
        {
            r.removeFromTop (12);
            auto row = r.removeFromTop (PillButton::kHeight);
            const int half = (row.getWidth() - 10) / 2;
            buy.setBounds (row.removeFromLeft (half));
            enterKey.setBounds (row.removeFromRight (half));
            r.removeFromTop (8);
            freeLink.setBounds (r.removeFromTop (kLinkH).withWidth (juce::jmin (w, freeLink.idealWidth())));
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (look.ground.interpolatedWith (juce::Colours::white, 0.035f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    }

    static constexpr int kPad = 14, kLinkH = 18;

    Look                 look;
    juce::String         productName;
    Para                 heading, state, details, sentence;
    PillButton           buy { true }, enterKey { false };
    TextLink             freeLink;
    juce::URL            buyTarget;                        // the shop page, or the renewal offer
    std::function<void()> onEnterKey, onFreeLicence;
    std::function<void (const juce::URL&)> launch;
};

//==============================================================================
// The pages' shared top: "← Back", then a title and a sentence.
struct PageTop
{
    void build (juce::Component& owner, const Look& look, const juce::String& titleText, const juce::String& sentenceText)
    {
        back.colour = look.accentHover;
        back.setButtonText (juce::String::fromUTF8 ("\xe2\x86\x90 ") + i18n::tr ("Back"));
        title.set (titleText, juce::Font (juce::FontOptions (17.0f, juce::Font::bold)), look.text);
        sentence.set (sentenceText, juce::Font (juce::FontOptions (13.0f)), look.ink);
        owner.addAndMakeVisible (back);
        owner.addAndMakeVisible (title);
        owner.addAndMakeVisible (sentence);
    }

    int heightFor (int w) const { return 20 + 8 + title.heightFor (w) + 6 + sentence.heightFor (w); }

    void layout (juce::Rectangle<int>& r)
    {
        const int w = r.getWidth();
        back.setBounds (r.removeFromTop (20).withWidth (juce::jmin (w, back.idealWidth())));
        r.removeFromTop (8);
        title.setBounds (r.removeFromTop (title.heightFor (w)));
        r.removeFromTop (6);
        sentence.setBounds (r.removeFromTop (sentence.heightFor (w)));
    }

    TextLink back;
    Para     title, sentence;
};

//==============================================================================
/** "Enter key": paste the block (or drop the .txt it came in), press Activate, read the answer. The product's
    function checks and stores; the page only says what it answered. */
class EnterKeyPage final : public juce::Component,
                           public juce::FileDragAndDropTarget
{
public:
    EnterKeyPage (Look l, juce::String product) : look (l), productName (std::move (product))
    {
        top.build (*this, look, i18n::tr ("Enter your key"),
                   i18n::tr ("Paste the whole block from the e-mail, or drop the key file here. No internet needed."));
        top.back.onClick = [this] { if (onBack) onBack(); };

        box.setMultiLine (true, false);
        box.setReturnKeyStartsNewLine (true);
        box.setScrollbarsShown (true);
        styleEditor (box, look, juce::Font (juce::FontOptions (12.0f).withName (juce::Font::getDefaultMonospacedFontName())));
        box.setTextToShowWhenEmpty ("-----BEGIN " + productName.toUpperCase().removeCharacters (" ") + " KEY-----",
                                    look.ink.withAlpha (0.5f));
        addAndMakeVisible (box);

        activate.fill = look.accentB;
        activate.setButtonText (i18n::tr ("Activate"));
        activate.onClick = [this] { activateNow(); };
        addAndMakeVisible (activate);

        addAndMakeVisible (answer);
    }

    /** Hands the box's text to the product and shows its answer. */
    void activateNow()
    {
        if (! enterKey)
            return;
        const auto reading = enterKey (box.getText());
        if (! reading)
            return;
        showAnswer (*reading);
        if (reading->verdict == Verdict::valid && onLicensed)
            onLicensed();
    }

    void showAnswer (const Reading& r)
    {
        using i18n::fill;
        using i18n::tr;
        const juce::Font f (juce::FontOptions (13.0f, juce::Font::bold));
        const auto name = juce::String::fromUTF8 (r.key.name.c_str()).trim();
        switch (r.verdict)
        {
            case Verdict::valid:
                answer.set (name.isNotEmpty() ? fill (tr ("Licensed to {name}. Thank you."), { { "name", name } })
                                              : tr ("Licensed. Thank you."),
                            f, juce::Colour (0xff7be29a));
                break;
            case Verdict::otherProduct:
                answer.set (fill (tr ("This key is for {other}, not {product}."),
                                  { { "other", juce::String::fromUTF8 (r.key.product.c_str()) }, { "product", productName } }),
                            f, look.accentB);
                break;
            case Verdict::newerFormat:
                answer.set (fill (tr ("This key needs a newer version of {product}."), { { "product", productName } }),
                            f, look.accentB);
                break;
            case Verdict::damaged:
                answer.set (tr ("This key is damaged: paste the whole block again."), f, look.accentB);
                break;
        }
        resized();
    }

    void clearAnswer() { answer.setText ({}); }

    // A key file is a few hundred bytes of text; anything else dropped here is not one.
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (const auto& f : files)
            if (f.endsWithIgnoreCase (".txt"))
                return true;
        return false;
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        for (const auto& path : files)
        {
            const juce::File f (path);
            if (path.endsWithIgnoreCase (".txt") && f.existsAsFile() && f.getSize() <= 64 * 1024)
            {
                box.setText (f.loadFileAsString(), false);
                activateNow();   // dropping the file is the gesture: there is nothing left to press
                return;
            }
        }
    }

    int heightFor (int width) const
    {
        const int w = width - 2 * kPad;
        return kPad + top.heightFor (w) + 12 + kBoxH + 12 + PillButton::kHeight + 10 + juce::jmax (18, answer.heightFor (w)) + kPad;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (kPad);
        top.layout (r);
        r.removeFromTop (12);
        box.setBounds (r.removeFromTop (kBoxH));
        r.removeFromTop (12);
        activate.setBounds (r.removeFromTop (PillButton::kHeight).withWidth (juce::jmax (140, activate.idealWidth())));
        r.removeFromTop (10);
        answer.setBounds (r.removeFromTop (answer.heightFor (r.getWidth())));
    }

    void paint (juce::Graphics& g) override { g.fillAll (look.ground); }

    static constexpr int kPad = 6, kBoxH = 116;

    Look              look;
    juce::String      productName;
    PageTop           top;
    juce::TextEditor  box;
    PillButton        activate { true };
    Para              answer;
    std::function<std::optional<Reading> (const juce::String&)> enterKey;   // nullopt: nobody to ask any more
    std::function<void()> onBack, onLicensed;
};

//==============================================================================
/** "Ask for a free licence": which of the four, name, e-mail, an optional link and a few words; "Write the e-mail"
    opens the mail app with the letter filled in. Nothing is sent from the product. */
class FreeLicencePage final : public juce::Component
{
public:
    FreeLicencePage (Look l, juce::String product, juce::String to)
        : look (l), productName (std::move (product)), address (std::move (to))
    {
        top.build (*this, look, i18n::tr ("Ask for a free licence"),
                   i18n::tr ("Tell us which one is you. We answer with a key, usually within a day or two."));
        top.back.onClick = [this] { if (onBack) onBack(); };

        iAm.set (i18n::tr ("I am"), juce::Font (juce::FontOptions (12.0f, juce::Font::bold)), look.text.withAlpha (0.6f));
        addAndMakeVisible (iAm);

        static constexpr const char* kChoices[] { "a student", "a musician short on money",
                                                  "someone who posted a link to {product}", "sending you my music" };
        for (size_t i = 0; i < choices.size(); ++i)
        {
            auto& c = choices[i];
            c.setButtonText (i18n::fill (i18n::tr (kChoices[i]), { { "product", productName } }));
            c.setRadioGroupId (0x11ce);
            c.setColour (juce::ToggleButton::textColourId, look.ink);
            c.setColour (juce::ToggleButton::tickColourId, look.accentHover);
            c.setColour (juce::ToggleButton::tickDisabledColourId, look.ink.withAlpha (0.5f));
            c.onClick = [this] { update(); };
            addAndMakeVisible (c);
        }

        const juce::Font f (juce::FontOptions (13.0f));
        auto field = [&] (juce::TextEditor& e, const char* hint)
        {
            styleEditor (e, look, f);
            e.setTextToShowWhenEmpty (i18n::fill (i18n::tr (hint), { { "product", productName } }), look.ink.withAlpha (0.6f));
            e.onTextChange = [this] { update(); };
            addAndMakeVisible (e);
        };
        field (name,  "Your name");
        field (email, "E-mail for the key");
        field (link,  "A link: your school, your post or your music (optional)");
        field (words, "A few words (optional)");
        words.setMultiLine (true, true);
        words.setReturnKeyStartsNewLine (true);

        write.fill = look.accentB;
        write.setButtonText (i18n::tr ("Write the e-mail"));
        write.onClick = [this] { if (launch && write.isEnabled()) launch (juce::URL::createWithoutParsing (url())); };
        addAndMakeVisible (write);

        note.set (i18n::fill (i18n::tr ("Opens your mail app with this filled in. Nothing is sent from {product} itself."),
                              { { "product", productName } }),
                  juce::Font (juce::FontOptions (12.0f)), look.ink.withAlpha (0.8f));
        addAndMakeVisible (note);
        update();
    }

    int choice() const
    {
        for (size_t i = 0; i < choices.size(); ++i)
            if (choices[i].getToggleState())
                return (int) i;
        return -1;
    }

    /** The letter as the fields say it now. */
    Letter letter() const
    {
        return freeLicenceLetter (productName, choice(), name.getText(), email.getText(), link.getText(), words.getText());
    }

    juce::String url() const
    {
        const auto l = letter();
        return mailtoUrl (address, l.subject, l.body);
    }

    // The letter needs to say who it is for and where the key goes; the rest is optional.
    void update()
    {
        write.setEnabled (choice() >= 0 && name.getText().trim().isNotEmpty() && email.getText().trim().isNotEmpty());
    }

    int heightFor (int width) const
    {
        const int w = width - 2 * kPad;
        const int noteW = w - write.idealWidth() - 14;
        return kPad + top.heightFor (w) + 12 + iAm.heightFor (w) + 4 + (int) choices.size() * kChoiceH + 10
             + kFieldH + 6 + kFieldH + 6 + kWordsH + 12 + juce::jmax (PillButton::kHeight, note.heightFor (noteW)) + kPad;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (kPad);
        const int w = r.getWidth();
        top.layout (r);
        r.removeFromTop (12);
        iAm.setBounds (r.removeFromTop (iAm.heightFor (w)));
        r.removeFromTop (4);
        for (auto& c : choices)
            c.setBounds (r.removeFromTop (kChoiceH));
        r.removeFromTop (10);
        {
            auto row = r.removeFromTop (kFieldH);
            const int half = (row.getWidth() - 10) / 2;
            name.setBounds (row.removeFromLeft (half));
            email.setBounds (row.removeFromRight (half));
        }
        r.removeFromTop (6);
        link.setBounds (r.removeFromTop (kFieldH));
        r.removeFromTop (6);
        words.setBounds (r.removeFromTop (kWordsH));
        r.removeFromTop (12);
        auto row = r.removeFromTop (juce::jmax (PillButton::kHeight, note.heightFor (w - write.idealWidth() - 14)));
        write.setBounds (row.removeFromLeft (write.idealWidth()).withHeight (PillButton::kHeight));
        row.removeFromLeft (14);
        note.setBounds (row.withSizeKeepingCentre (row.getWidth(), note.heightFor (row.getWidth())));
    }

    void paint (juce::Graphics& g) override { g.fillAll (look.ground); }

    static constexpr int kPad = 6, kChoiceH = 22, kFieldH = 28, kWordsH = 56;

    Look                       look;
    juce::String               productName, address;
    PageTop                    top;
    Para                       iAm, note;
    std::array<juce::ToggleButton, 4> choices;
    juce::TextEditor           name, email, link, words;
    PillButton                 write { true };
    std::function<void()>      onBack;
    std::function<void (const juce::URL&)> launch;
};

} // namespace felitronics::appkit::licence

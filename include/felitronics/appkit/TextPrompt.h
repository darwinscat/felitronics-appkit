// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::textPrompt — one-line modal text prompt (OK/Enter · Cancel/Esc), moved
// verbatim from orbit-capture's UiSupport.h (its new-session prompt / gear-list managers / add-model
// flow). Header-only; the consumer supplies JUCE (juce_gui_basics).
//==============================================================================

#include <felitronics/appkit/Brand.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace felitronics::appkit
{

inline void textPrompt (const juce::String& title, const juce::String& initial,
                        std::function<void (juce::String)> onOk)
{
    auto* w = new juce::AlertWindow (title, "", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("v", initial);
    if (auto* te = w->getTextEditor ("v"))                      // make it obviously an editor (boxed, filled)
    {
        te->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff2b2f36));
        te->setColour (juce::TextEditor::outlineColourId, juce::Colours::grey);
        te->setColour (juce::TextEditor::focusedOutlineColourId, brand::violet);
        te->setColour (juce::TextEditor::textColourId, juce::Colours::white);
        te->setSelectAllWhenFocused (true);
    }
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, onOk] (int r)
    {
        const juce::String v = w->getTextEditorContents ("v").trim();   // strip leading/trailing space/tab/newline
        if (r == 1 && v.isNotEmpty() && onOk) onOk (v);
    }), true);
    w->toFront (true);                                          // above any (non-native) host dialog
}

} // namespace felitronics::appkit

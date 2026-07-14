// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ChromeMetrics.h"   // ChromeTheme
#include "FlatButtons.h"     // FlatItem — the preset name button

#include <functional>
#include <utility>

namespace felitronics::appkit::chrome
{

//==============================================================================
// PresetModel — the consumer-owned preset state pushed into the cell. LEAN by design: the cell shows
// only the current name; the consumer builds the whole menu (PresetActions::showList) — sections,
// stable IDs, save-as, delete, etc. all live there.
struct PresetModel
{
    juce::String currentName;   // the loaded preset's display name
};

// PresetActions — the cell's single gesture: the consumer opens the preset menu. Load AND
// Save/Import/Export all live INSIDE that menu.
struct PresetActions
{
    std::function<void ()> showList;   // preset-name click → the consumer's preset menu
};

//==============================================================================
// PresetCell — the preset name as one chrome cell, driven by a consumer model + a single action
// callback. The name is a flat text item; clicking it opens the consumer's preset menu (which is
// where Load / Save / Import / Export live).
class PresetCell final : public juce::Component
{
public:
    explicit PresetCell (const ChromeTheme& theme) : theme_ (theme)
    {
        preset.theme = theme_;
        preset.setButtonText ("Default");
        preset.onClick = [this] { if (actions.showList) actions.showList(); };
        addAndMakeVisible (preset);
    }

    void setActions (PresetActions a) { actions = std::move (a); }

    void setModel (const PresetModel& m) { model = m; preset.setButtonText (m.currentName); }
    void setCurrentName (const juce::String& n) { model.currentName = n; preset.setButtonText (n); }
    juce::String currentName() const { return model.currentName; }

    // The component a consumer popup (the preset menu) should anchor to — the name item.
    juce::Component* nameAnchor() { return &preset; }

    int fixedWidth() const { return kPresetW; }

    void resized() override
    {
        preset.setBounds (0, 4, getWidth(), getHeight() - 8);   // vInset 4, full cell width
    }

private:
    static constexpr int kPresetW = 104;

    ChromeTheme   theme_;
    FlatItem      preset;
    PresetActions actions;
    PresetModel   model;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetCell)
};

} // namespace felitronics::appkit::chrome

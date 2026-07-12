// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <utility>

//==============================================================================
// felitronics::appkit::launchCallOut — a CallOutBox popover parented to the anchor's TOP-LEVEL
// component (the plugin editor), NOT the desktop (the nullptr overload). A desktop call-out
// outlives the editor: close the plugin window with it open and it orphans on screen with no way
// to dismiss it. As a child of the top-level editor it is destroyed with the window.
// (`areaToPointTo` must be in the parent's coordinates — hence getLocalArea.)
//
// Consolidated from the identical showPopup() blocks in OrbitCab's VersionBadge/PerfBadge; used by
// this repo's VersionBadge.h + PerfBadge.h and available to any other appkit popover.
//==============================================================================
namespace felitronics::appkit
{

inline juce::CallOutBox& launchCallOut (juce::Component& anchor, std::unique_ptr<juce::Component> panel)
{
    if (auto* top = anchor.getTopLevelComponent())
        return juce::CallOutBox::launchAsynchronously (std::move (panel),
                                                       top->getLocalArea (&anchor, anchor.getLocalBounds()), top);
    return juce::CallOutBox::launchAsynchronously (std::move (panel), anchor.getScreenBounds(), nullptr);
}

} // namespace felitronics::appkit

#pragma once

#include "fnvxr_desktop_assist_authority.h"

namespace fnvxr::engine
{
// Desktop assist normally has no command or input path.  The supervisor may
// opt into exactly one recovery action for an unattended, desktop-only
// acceptance run: loading the fixed, pre-existing recovery save.  The plugin
// never treats the general command mailbox as an authority in this profile.
enum class DesktopAssistAutomationAction : unsigned char
{
    None = 0,
    LoadFixedRecoverySave,
};

constexpr bool desktopAssistAutomationActionIsNarrow(
    DesktopAssistAutomationAction action) noexcept
{
    return action == DesktopAssistAutomationAction::LoadFixedRecoverySave;
}

constexpr bool desktopAssistAutomationAuthorized(
    const DesktopAssistCameraRequest& cameraRequest,
    bool explicitlyRequested,
    DesktopAssistAutomationAction action) noexcept
{
    return explicitlyRequested
        && desktopAssistCameraRequestIsNarrow(cameraRequest)
        && desktopAssistAutomationActionIsNarrow(action);
}
}

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class SWindow;

// What a dialog holding the editor says, for a call refused EDITOR_MODAL_ACTIVE: its title, the words outside its
// buttons, and each button's label with whether it is enabled. Read only; nothing here presses a button.
namespace McpModalDialog
{
// {title, message, buttons[{label, enabled}]}, read once per window (each queued call's refusal reuses it); null for
// no window.
TSharedPtr<FJsonObject> Describe(const TSharedPtr<SWindow>& Window);
}

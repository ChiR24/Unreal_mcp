#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UEdGraphPin;

// What connect_pins changed beyond the new link, written onto its reply: replacedLinks (the links a pin that
// holds one dropped to take it) and a warning when an input now holds several links (a Target pin runs the
// call on each object wired into it).
void McpReportPinLinkChanges(const TSharedPtr<FJsonObject>& Result, const UEdGraphPin* FromPin, const UEdGraphPin* ToPin,
                             const TArray<UEdGraphPin*>& FromBefore, const TArray<UEdGraphPin*>& ToBefore);

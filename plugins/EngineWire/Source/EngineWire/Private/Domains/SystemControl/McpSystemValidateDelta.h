#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpValidateDelta
{
/**
 * The engine's Data Validation of the assets the request RequestId changed, as the editor runs it when it saves an asset:
 * each error and warning it reports goes into Result's warnings. Only assets already loaded are checked; nothing loads.
 * The change watch says the same of the actors a call changed (placement, Map Check).
 */
void AddWarnings(const FString& RequestId, const TSharedPtr<FJsonObject>& Result);
} // namespace McpValidateDelta

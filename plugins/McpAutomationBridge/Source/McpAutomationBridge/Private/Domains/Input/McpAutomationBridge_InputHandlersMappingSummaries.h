#pragma once

#include "Templates/SharedPointer.h"

class FJsonObject;
class UInputAction;
class UInputMappingContext;
class UObject;

namespace McpInputHandlers
{
void AddInputMappingSummary(
    TSharedPtr<FJsonObject> Result,
    const UInputMappingContext* Context,
    const UInputAction* InAction);

// The receipt's changes[] is read from changedAssets: name the asset this call saved, or none (nullptr) when it
// changed nothing, so a caller diffing changes[] neither misses an edited mapping context nor sees an untouched one.
void SetInputChangedAsset(const TSharedPtr<FJsonObject>& Result, const UObject* Asset, bool bSaved);
}

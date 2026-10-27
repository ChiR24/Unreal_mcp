#pragma once

#include "Templates/SharedPointer.h"

class FJsonObject;
class UInputAction;
class UInputMappingContext;
class UObject;

namespace McpInputHandlers
{
// actionMappings: the keys InAction is still mapped to in Context. Named apart from get_input_info's mappingCount,
// the whole context's count: after a remove_mapping, "mappingCount": 0 read as an emptied context.
void AddInputMappingSummary(
    TSharedPtr<FJsonObject> Result,
    const UInputMappingContext* Context,
    const UInputAction* InAction);

// The receipt's changes[] is read from changedAssets: name the asset this call saved, or none (nullptr) when it
// changed nothing, so a caller diffing changes[] neither misses an edited mapping context nor sees an untouched one.
void SetInputChangedAsset(const TSharedPtr<FJsonObject>& Result, const UObject* Asset, bool bSaved);
}

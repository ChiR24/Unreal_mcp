#pragma once

#include "CoreMinimal.h"

class UInputAction;
class UInputMappingContext;

namespace McpInputHandlers
{
#if WITH_EDITOR
FString NormalizeInputAssetPathForLoad(const FString& RawPath);
UInputAction* LoadInputActionAsset(const FString& RawPath, FString& OutNormalizedPath);
UInputMappingContext* LoadInputMappingContextAsset(
    const FString& RawPath,
    FString& OutNormalizedPath);
UObject* LoadInputObjectAsset(const FString& RawPath, FString& OutNormalizedPath);
#endif
}

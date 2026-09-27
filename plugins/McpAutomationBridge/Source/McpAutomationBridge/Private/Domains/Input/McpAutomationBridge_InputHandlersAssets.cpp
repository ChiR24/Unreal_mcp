#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Input/McpAutomationBridge_InputHandlersAssetResolution.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Misc/PackageName.h"

namespace McpInputHandlers
{
namespace
{
FString NormalizeInputAssetPathForLoad(const FString& RawPath)
{
    FString CleanPath = RawPath.TrimStartAndEnd();
    int32 QuoteStart = INDEX_NONE;
    int32 QuoteEnd = INDEX_NONE;
    if (CleanPath.FindChar(TEXT('\''), QuoteStart) && CleanPath.FindLastChar(TEXT('\''), QuoteEnd) && QuoteEnd > QuoteStart)
    {
        CleanPath = CleanPath.Mid(QuoteStart + 1, QuoteEnd - QuoteStart - 1);
    }

    int32 DotIndex = INDEX_NONE;
    FString PackagePath = CleanPath;
    if (CleanPath.FindChar(TEXT('.'), DotIndex))
    {
        PackagePath = CleanPath.Left(DotIndex);
    }

    FString SanitizedPackagePath = SanitizeProjectRelativePath(PackagePath);
    if (SanitizedPackagePath.IsEmpty())
    {
        return FString();
    }

    return DotIndex == INDEX_NONE ? SanitizedPackagePath : SanitizedPackagePath + CleanPath.Mid(DotIndex);
}

// OutNormalizedPath keeps the caller's (sanitized) form; a package path loads its same-named asset. StaticLoadObject
// rather than UEditorAssetLibrary, which refuses every call during PIE.
template <typename TAsset>
TAsset* LoadInputAsset(const FString& RawPath, FString& OutNormalizedPath)
{
    OutNormalizedPath = NormalizeInputAssetPathForLoad(RawPath);
    if (OutNormalizedPath.IsEmpty())
    {
        return nullptr;
    }
    const FString ObjectPath = OutNormalizedPath.Contains(TEXT("."))
        ? OutNormalizedPath
        : OutNormalizedPath + TEXT(".") + FPackageName::GetShortName(OutNormalizedPath);
    return Cast<TAsset>(StaticLoadObject(TAsset::StaticClass(), nullptr, *ObjectPath, nullptr, LOAD_NoWarn));
}
}

UInputAction* LoadInputActionAsset(const FString& RawPath, FString& OutNormalizedPath)
{
    return LoadInputAsset<UInputAction>(RawPath, OutNormalizedPath);
}

UInputMappingContext* LoadInputMappingContextAsset(const FString& RawPath, FString& OutNormalizedPath)
{
    return LoadInputAsset<UInputMappingContext>(RawPath, OutNormalizedPath);
}

UObject* LoadInputObjectAsset(const FString& RawPath, FString& OutNormalizedPath)
{
    return LoadInputAsset<UObject>(RawPath, OutNormalizedPath);
}
}

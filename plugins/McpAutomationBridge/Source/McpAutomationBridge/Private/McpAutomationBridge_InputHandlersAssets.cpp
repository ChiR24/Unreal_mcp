#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_InputHandlersAssetResolution.h"

#include "EditorAssetLibrary.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "McpAutomationBridgeHelpers.h"
#include "Misc/PackageName.h"

namespace McpInputHandlers
{
#if WITH_EDITOR
namespace
{
template <typename TAsset>
TAsset* LoadInputAsset(const FString& RawPath, FString& OutNormalizedPath)
{
    OutNormalizedPath = NormalizeInputAssetPathForLoad(RawPath);
    if (OutNormalizedPath.IsEmpty())
    {
        return nullptr;
    }

    if (UObject* Loaded = UEditorAssetLibrary::LoadAsset(OutNormalizedPath))
    {
        if (TAsset* Typed = Cast<TAsset>(Loaded))
        {
            return Typed;
        }
    }

    TArray<FString> Candidates;
    Candidates.Add(OutNormalizedPath);
    if (!OutNormalizedPath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(OutNormalizedPath);
        Candidates.Add(FString::Printf(TEXT("%s.%s"), *OutNormalizedPath, *AssetName));
    }

    for (const FString& Candidate : Candidates)
    {
        if (UObject* Loaded = StaticLoadObject(TAsset::StaticClass(), nullptr, *Candidate))
        {
            if (TAsset* Typed = Cast<TAsset>(Loaded))
            {
                OutNormalizedPath = Candidate;
                return Typed;
            }
        }
    }

    return nullptr;
}
}

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
    OutNormalizedPath = NormalizeInputAssetPathForLoad(RawPath);
    UObject* Asset = OutNormalizedPath.IsEmpty() ? nullptr : UEditorAssetLibrary::LoadAsset(OutNormalizedPath);
    return (!Asset && !OutNormalizedPath.IsEmpty())
        ? StaticLoadObject(UObject::StaticClass(), nullptr, *OutNormalizedPath)
        : Asset;
}
#endif
}

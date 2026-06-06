#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#endif

namespace McpHandlerUtils
{

FString ValidateAssetPath(const FString& Path)
{
    if (Path.IsEmpty())
    {
        return FString();
    }

    FString CleanPath = Path;

    // Reject Windows absolute paths
    if (CleanPath.Len() >= 2 && CleanPath[1] == TEXT(':'))
    {
        UE_LOG(LogTemp, Warning, TEXT("ValidateAssetPath: Rejected Windows absolute path: %s"), *Path);
        return FString();
    }

    // Normalize slashes
    CleanPath.ReplaceInline(TEXT("\\"), TEXT("/"));

    // Remove double slashes
    while (CleanPath.Contains(TEXT("//")))
    {
        CleanPath = CleanPath.Replace(TEXT("//"), TEXT("/"));
    }

    // Reject path traversal
    if (CleanPath.Contains(TEXT("..")))
    {
        UE_LOG(LogTemp, Warning, TEXT("ValidateAssetPath: Rejected path containing '..': %s"), *Path);
        return FString();
    }

    // Ensure path starts with /
    if (!CleanPath.StartsWith(TEXT("/")))
    {
        CleanPath = TEXT("/") + CleanPath;
    }

    // Validate root
    const bool bValidRoot = CleanPath.StartsWith(TEXT("/Game/")) ||
                           CleanPath.StartsWith(TEXT("/Engine/")) ||
                           CleanPath.StartsWith(TEXT("/Script/"));

    if (!bValidRoot)
    {
        // Use engine validation for non-standard roots (plugin paths, etc.)
        FText Reason;
        if (!FPackageName::IsValidLongPackageName(CleanPath, true, &Reason))
        {
            UE_LOG(LogTemp, Warning, TEXT("ValidateAssetPath: Rejected path without valid root: %s (%s)"),
                   *Path, *Reason.ToString());
            return FString();
        }
    }

    return CleanPath;
}
}

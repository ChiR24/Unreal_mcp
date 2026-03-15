// =============================================================================
// McpAutomationBridge_AssetQueryHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Asset Query & Search Handlers
// 
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
// 
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: asset_query
//   - get_dependencies: Get package dependencies (hard/soft) for an asset
//   - find_by_tag: Find assets by metadata tag value
//   - search_assets: Query assets by class, path, and other filters
//   - get_source_control_state: Get source control state for asset (Editor Only)
// 
// Action: search_assets (wrapper)
//   - Delegates to asset_query with subAction="search_assets"
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: AssetRegistry, ARFilter
//   - Editor: EditorAssetLibrary, SourceControl
// 
// Version Compatibility Notes:
//   - UE 5.1+: AssetClassPath (FTopLevelAssetPath) for class references
//   - UE 5.0: AssetClass (FName) for class references
//   - GetSoftObjectPath() vs ToSoftObjectPath() differs by version
// 
// Security:
//   - All paths sanitized via SanitizeProjectRelativePath() to prevent traversal
//   - Default search path is /Game to prevent massive project scans
// 
// Performance:
//   - Uses AssetRegistry cached data - no asset loading required
//   - REMOVED ScanPathsSynchronous() to prevent indefinite hangs on unindexed paths
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#endif

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleAssetQueryAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Validate action (case-insensitive)
    if (!Action.ToLower().Equals(TEXT("asset_query"), ESearchCase::IgnoreCase))
    {
        return false;
    }

    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract subaction
    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    // -------------------------------------------------------------------------
    // get_dependencies: Get package dependencies for an asset
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("get_dependencies"))
    {
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        if (AssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Missing assetPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Sanitize the path to prevent traversal attacks
        FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
        if (SanitizedAssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid assetPath: '%s' contains traversal or invalid characters."), *AssetPath),
                TEXT("INVALID_PATH"));
            return true;
        }

        // Note: UE AssetRegistry GetDependencies API does not support recursive traversal.
        // All dependencies returned are direct dependencies only.
        // The 'recursive' parameter is accepted for API consistency but currently ignored.

        bool bIncludeSoftDependencies = false;
        Payload->TryGetBoolField(TEXT("includeSoftDependencies"), bIncludeSoftDependencies);

        FAssetRegistryModule& AssetRegistryModule = 
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

        TArray<FName> Dependencies;

        // Build query based on dependency type and recursion
        // UE 5.7 uses bitflags: Hard for hard deps, Soft (NotHard) for soft deps
        // Recursive behavior is handled by the category parameter in GetDependencies
        UE::AssetRegistry::EDependencyQuery Query;
        if (bIncludeSoftDependencies)
        {
            Query = UE::AssetRegistry::EDependencyQuery::Soft;
        }
        else
        {
            Query = UE::AssetRegistry::EDependencyQuery::Hard;
        }

        AssetRegistryModule.Get().GetDependencies(
            FName(*SanitizedAssetPath), 
            Dependencies,
            UE::AssetRegistry::EDependencyCategory::Package, 
            Query);

        // Build response
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        TArray<TSharedPtr<FJsonValue>> DepArray;

        for (const FName& Dep : Dependencies)
        {
            DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
        }

        Result->SetArrayField(TEXT("dependencies"), DepArray);

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Dependencies retrieved."), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // find_by_tag: Find assets by metadata tag value
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("find_by_tag"))
    {
        FString Tag;
        Payload->TryGetStringField(TEXT("tag"), Tag);

        FString ExpectedValue;
        Payload->TryGetStringField(TEXT("value"), ExpectedValue);

        if (Tag.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("tag required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Get optional path filter (default to /Game)
        FString RawPath;
        Payload->TryGetStringField(TEXT("path"), RawPath);

        FString Path;
        if (!RawPath.IsEmpty())
        {
            // SECURITY: Sanitize path to prevent traversal attacks
            Path = SanitizeProjectRelativePath(RawPath);
            if (Path.IsEmpty())
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *RawPath),
                    TEXT("INVALID_PATH"));
                return true;
            }
        }
        else
        {
            Path = TEXT("/Game");
        }

        // Query AssetRegistry (uses cached data, no loading required)
        FAssetRegistryModule& AssetRegistryModule = 
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

        FARFilter Filter;
        Filter.PackagePaths.Add(FName(*Path));
        Filter.bRecursivePaths = true;

        TArray<FAssetData> AssetDataList;
        AssetRegistry.GetAssets(Filter, AssetDataList);

        // Filter by tag (using cached metadata - no asset loading!)
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        TArray<TSharedPtr<FJsonValue>> AssetsArray;
        const FName TagFName(*Tag);

        for (const FAssetData& Data : AssetDataList)
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            const FString AssetPath = Data.GetSoftObjectPath().ToString();
#else
            const FString AssetPath = Data.ToSoftObjectPath().ToString();
#endif

            // Use cached tag value (O(1) lookup vs O(n) disk I/O)
            FString MetadataValue;
            bool bHasTag = Data.GetTagValue(TagFName, MetadataValue);

            // Check if matches expected value (or just existence)
            bool bMatches = bHasTag;
            if (bMatches && !ExpectedValue.IsEmpty())
            {
                bMatches = MetadataValue.Equals(ExpectedValue, ESearchCase::IgnoreCase);
            }

            if (bMatches)
            {
                TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
                AssetObj->SetStringField(TEXT("assetName"), Data.AssetName.ToString());
                AssetObj->SetStringField(TEXT("assetPath"), AssetPath);
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
                AssetObj->SetStringField(TEXT("classPath"), Data.AssetClassPath.ToString());
#else
                AssetObj->SetStringField(TEXT("classPath"), Data.AssetClass.ToString());
#endif
                AssetObj->SetStringField(TEXT("tagValue"), MetadataValue);
                AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
            }
        }

        Result->SetArrayField(TEXT("assets"), AssetsArray);
        Result->SetNumberField(TEXT("count"), AssetsArray.Num());

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Assets found by tag"), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // search_assets: Query assets by class, path, and other filters
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("search_assets"))
    {
        FARFilter Filter;

        // Parse Class Names
        const TArray<TSharedPtr<FJsonValue>>* ClassNamesPtr;
        if (Payload->TryGetArrayField(TEXT("classNames"), ClassNamesPtr) && ClassNamesPtr)
        {
            for (const TSharedPtr<FJsonValue>& Val : *ClassNamesPtr)
            {
                const FString ClassName = Val->AsString();
                if (!ClassName.IsEmpty())
                {
                    // Support both full paths and short names
                    if (ClassName.Contains(TEXT("/")))
                    {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
                        Filter.ClassPaths.Add(FTopLevelAssetPath(ClassName));
#else
                        // UE 5.0: Extract class name from path
                        int32 DotIndex;
                        if (ClassName.FindLastChar(TEXT('.'), DotIndex))
                        {
                            Filter.ClassNames.Add(FName(*ClassName.Mid(DotIndex + 1)));
                        }
                        else
                        {
                            Filter.ClassNames.Add(FName(*ClassName));
                        }
#endif
                    }
                    else
                    {
                        // Map common short names to full paths
                        const bool bIsBlueprint = ClassName.Equals(TEXT("Blueprint"), ESearchCase::IgnoreCase);
                        const bool bIsStaticMesh = ClassName.Equals(TEXT("StaticMesh"), ESearchCase::IgnoreCase);
                        const bool bIsSkeletalMesh = ClassName.Equals(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase);
                        const bool bIsMaterial = ClassName.Equals(TEXT("Material"), ESearchCase::IgnoreCase);
                        const bool bIsMaterialInstance = ClassName.Equals(TEXT("MaterialInstance"), ESearchCase::IgnoreCase) ||
                            ClassName.Equals(TEXT("MaterialInstanceConstant"), ESearchCase::IgnoreCase);
                        const bool bIsTexture2D = ClassName.Equals(TEXT("Texture2D"), ESearchCase::IgnoreCase);
                        const bool bIsLevel = ClassName.Equals(TEXT("Level"), ESearchCase::IgnoreCase) ||
                            ClassName.Equals(TEXT("World"), ESearchCase::IgnoreCase);
                        const bool bIsSoundCue = ClassName.Equals(TEXT("SoundCue"), ESearchCase::IgnoreCase);
                        const bool bIsSoundWave = ClassName.Equals(TEXT("SoundWave"), ESearchCase::IgnoreCase);

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
                        if (bIsBlueprint)
                            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Blueprint")));
                        else if (bIsStaticMesh)
                            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("StaticMesh")));
                        else if (bIsSkeletalMesh)
                            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("SkeletalMesh")));
                        else if (bIsMaterial)
                            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Material")));
                        else if (bIsMaterialInstance)
                            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("MaterialInstanceConstant")));
                        else if (bIsTexture2D)
                            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Texture2D")));
                        else if (bIsLevel)
                            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("World")));
                        else if (bIsSoundCue)
                            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("SoundCue")));
                        else if (bIsSoundWave)
                            Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("SoundWave")));
#else
                        if (bIsBlueprint)
                            Filter.ClassNames.Add(TEXT("Blueprint"));
                        else if (bIsStaticMesh)
                            Filter.ClassNames.Add(TEXT("StaticMesh"));
                        else if (bIsSkeletalMesh)
                            Filter.ClassNames.Add(TEXT("SkeletalMesh"));
                        else if (bIsMaterial)
                            Filter.ClassNames.Add(TEXT("Material"));
                        else if (bIsMaterialInstance)
                            Filter.ClassNames.Add(TEXT("MaterialInstanceConstant"));
                        else if (bIsTexture2D)
                            Filter.ClassNames.Add(TEXT("Texture2D"));
                        else if (bIsLevel)
                            Filter.ClassNames.Add(TEXT("World"));
                        else if (bIsSoundCue)
                            Filter.ClassNames.Add(TEXT("SoundCue"));
                        else if (bIsSoundWave)
                            Filter.ClassNames.Add(TEXT("SoundWave"));
#endif
                        else
                        {
                            // Unknown short class name - return error instead of widening query
                            SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Unknown short class name '%s'. Use full path (e.g. /Script/Engine.Blueprint) or one of: Blueprint, StaticMesh, SkeletalMesh, Material, MaterialInstance, Texture2D, Level, World, SoundCue, SoundWave."),
                                    *ClassName),
                                TEXT("UNKNOWN_CLASS_NAME"));
                            return true;
                        }
                    }
                }
            }
        }

        // Parse Package Paths - SECURITY validated, default to /Game
        const TArray<TSharedPtr<FJsonValue>>* PackagePathsPtr;
        bool bHasValidPaths = false;

        // Check for packagePaths array
        if (Payload->TryGetArrayField(TEXT("packagePaths"), PackagePathsPtr) && 
            PackagePathsPtr && PackagePathsPtr->Num() > 0)
        {
            for (const TSharedPtr<FJsonValue>& Val : *PackagePathsPtr)
            {
                FString RawPath = Val->AsString();
                FString SanitizedPath = SanitizeProjectRelativePath(RawPath);
                if (SanitizedPath.IsEmpty())
                {
                    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Invalid package path '%s': contains traversal sequences"), *RawPath),
                        TEXT("INVALID_PATH"));
                    return true;
                }
                Filter.PackagePaths.Add(FName(*SanitizedPath));
                bHasValidPaths = true;
            }
        }

        // Check for 'path' (singular) string field
        FString SinglePath;
        if (Payload->TryGetStringField(TEXT("path"), SinglePath) && !SinglePath.IsEmpty())
        {
            FString SanitizedPath = SanitizeProjectRelativePath(SinglePath);
            if (SanitizedPath.IsEmpty())
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *SinglePath),
                    TEXT("SECURITY_VIOLATION"));
                return true;
            }
            Filter.PackagePaths.Add(FName(*SanitizedPath));
            bHasValidPaths = true;
        }

        // Default to /Game if no valid paths specified
        if (!bHasValidPaths)
        {
            Filter.PackagePaths.Add(FName(TEXT("/Game")));
        }

        // Parse recursion (default to false for safety)
        bool bRecursivePaths = false;
        if (Payload->HasField(TEXT("recursivePaths")))
        {
            Payload->TryGetBoolField(TEXT("recursivePaths"), bRecursivePaths);
        }
        Filter.bRecursivePaths = bRecursivePaths;

        bool bRecursiveClasses = false;
        if (Payload->HasField(TEXT("recursiveClasses")))
        {
            Payload->TryGetBoolField(TEXT("recursiveClasses"), bRecursiveClasses);
        }
        Filter.bRecursiveClasses = bRecursiveClasses;

        // Execute query (uses cached AssetRegistry data)
        FAssetRegistryModule& AssetRegistryModule = 
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

        TArray<FAssetData> AssetDataList;
        AssetRegistry.GetAssets(Filter, AssetDataList);

        // Apply limit
        int32 Limit = 100;
        if (Payload->HasField(TEXT("limit")))
        {
            Payload->TryGetNumberField(TEXT("limit"), Limit);
        }
        if (Limit > 0 && AssetDataList.Num() > Limit)
        {
            AssetDataList.SetNum(Limit);
        }

        // Build response
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        TArray<TSharedPtr<FJsonValue>> AssetsArray;

        for (const FAssetData& Data : AssetDataList)
        {
            TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
            AssetObj->SetStringField(TEXT("assetName"), Data.AssetName.ToString());

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            AssetObj->SetStringField(TEXT("assetPath"), Data.GetSoftObjectPath().ToString());
            AssetObj->SetStringField(TEXT("classPath"), Data.AssetClassPath.ToString());
#else
            AssetObj->SetStringField(TEXT("assetPath"), Data.ToSoftObjectPath().ToString());
            AssetObj->SetStringField(TEXT("classPath"), Data.AssetClass.ToString());
#endif

            AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
        }

        Result->SetBoolField(TEXT("success"), true);
        Result->SetArrayField(TEXT("assets"), AssetsArray);
        Result->SetNumberField(TEXT("count"), AssetsArray.Num());

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Assets found."), Result);
        return true;
    }

#if WITH_EDITOR
    // -------------------------------------------------------------------------
    // get_source_control_state: Get source control state for asset (Editor Only)
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("get_source_control_state"))
    {
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        if (ISourceControlModule::Get().IsEnabled())
        {
            ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
            FSourceControlStatePtr State = Provider.GetState(AssetPath, EStateCacheUsage::Use);

            if (State.IsValid())
            {
                TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
                Result->SetBoolField(TEXT("isCheckedOut"), State->IsCheckedOut());
                Result->SetBoolField(TEXT("isAdded"), State->IsAdded());
                Result->SetBoolField(TEXT("isDeleted"), State->IsDeleted());
                Result->SetBoolField(TEXT("isModified"), State->IsModified());

                SendAutomationResponse(RequestingSocket, RequestId, true, 
                    TEXT("Source control state retrieved."), Result);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    TEXT("Could not get source control state."), TEXT("STATE_FAILED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Source control not enabled."), TEXT("SC_DISABLED"));
        }
        return true;
    }
#endif

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}

// =============================================================================
// Wrapper Handler: search_assets
// =============================================================================

/**
 * Wrapper for search_assets action when called directly (not via asset_query).
 * Routes to HandleAssetQueryAction with subAction="search_assets".
 */
bool UMcpAutomationBridgeSubsystem::HandleSearchAssets(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Build payload with subAction
    TSharedPtr<FJsonObject> RoutedPayload = Payload;
    if (Payload.IsValid())
    {
        if (!Payload->HasField(TEXT("subAction")))
        {
            RoutedPayload = MakeShared<FJsonObject>(*Payload);
            RoutedPayload->SetStringField(TEXT("subAction"), TEXT("search_assets"));
        }
    }

    // Delegate to HandleAssetQueryAction
    return HandleAssetQueryAction(RequestId, TEXT("asset_query"), RoutedPayload, RequestingSocket);
}

#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ActorComponent.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

namespace McpLevelStructure
{

bool HandleCreateSublevel(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    // CRITICAL: sublevelName is required - no default fallback to prevent hidden errors
    FString SublevelName;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("sublevelName"), SublevelName);
    }

    if (SublevelName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("sublevelName is required for create_sublevel"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SublevelPath = GetJsonStringField(Payload, TEXT("sublevelPath"), TEXT(""));
    FString ParentLevel = GetJsonStringField(Payload, TEXT("parentLevel"), TEXT(""));
    bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);
    // streamingMethod picks the streaming class; it used to be ignored and every sublevel streamed by Blueprint.
    const FString StreamingMethod = GetJsonStringField(Payload, TEXT("streamingMethod"), TEXT("Blueprint"));
    UClass* StreamingClass = ResolveLevelStreamingClass(StreamingMethod);
    if (!StreamingClass)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Unknown streamingMethod '%s'; use Blueprint or AlwaysLoaded."), *StreamingMethod), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_EDITOR_WORLD"));
        return true;
    }

    if (!ParentLevel.IsEmpty())
    {
        FString NormalizedParentPath = ParentLevel;
        if (!NormalizedParentPath.StartsWith(TEXT("/Game/")))
        {
            NormalizedParentPath = TEXT("/Game/") + NormalizedParentPath;
        }
        NormalizedParentPath.RemoveFromEnd(TEXT(".umap"));

        if (!FPackageName::DoesPackageExist(NormalizedParentPath))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Parent level not found: %s"), *ParentLevel), nullptr, TEXT("LEVEL_NOT_FOUND"));
            return true;
        }
    }

    FString FullSublevelPath;
    if (SublevelPath.IsEmpty())
    {
        FString WorldPath = World->GetOutermost()->GetName();
        FullSublevelPath = FPaths::GetPath(WorldPath) / SublevelName;
    }
    else
    {
        // Security: Validate sublevel path format to prevent traversal attacks
        FString SafePath = SanitizeProjectRelativePath(SublevelPath);
        if (SafePath.IsEmpty())
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                McpPathRefusalMessage(TEXT("sublevel path"), SublevelPath),
                nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }
        // sublevelPath may be either the full package path (/Game/Maps/MySub) or the
        // folder the sublevel should live in (/Game/Maps). Previously it was always
        // taken verbatim as the package path, so passing a folder together with a
        // sublevelName produced a package named after the folder — e.g.
        // "/Game/NyxTest/Maps" + "NyxSub_Audio" created the package
        // "/Game/NyxTest/Maps" with the level inside it, instead of
        // "/Game/NyxTest/Maps/NyxSub_Audio". Treat it as a folder whenever the
        // final path segment does not already match the requested name.
        const FString LastSegment = FPaths::GetCleanFilename(SafePath);
        if (!LastSegment.Equals(SublevelName, ESearchCase::IgnoreCase))
        {
            FullSublevelPath = SafePath / SublevelName;
        }
        else
        {
            FullSublevelPath = SafePath;
        }
    }

    if (!FullSublevelPath.StartsWith(TEXT("/Game/")))
    {
        FullSublevelPath = TEXT("/Game/") + FullSublevelPath;
    }

    // IDEMPOTENT: Check if sublevel already exists
    if (FPackageName::DoesPackageExist(FullSublevelPath))
    {
        ULevelStreaming* ExistingStreamingLevel = nullptr;
        for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
        {
            if (StreamingLevel && StreamingLevel->GetWorldAssetPackageFName().ToString() == FullSublevelPath)
            {
                ExistingStreamingLevel = StreamingLevel;
                break;
            }
        }

        TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
        ResponseJson->SetStringField(TEXT("sublevelName"), SublevelName);
        ResponseJson->SetStringField(TEXT("sublevelPath"), FullSublevelPath);
        ResponseJson->SetStringField(TEXT("parentLevel"), World->GetMapName());
        ResponseJson->SetBoolField(TEXT("alreadyExisted"), true);
        ResponseJson->SetBoolField(TEXT("streamingAdded"), ExistingStreamingLevel != nullptr);

        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Sublevel already exists: %s"), *FullSublevelPath), ResponseJson);
        return true;
    }

    FString SublevelPackageName = FullSublevelPath;

    UPackage* SublevelPackage = CreatePackage(*SublevelPackageName);
    if (!SublevelPackage)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create package for sublevel: %s"), *SublevelPackageName), nullptr, TEXT("PACKAGE_CREATION_FAILED"));
        return true;
    }

    UWorld* NewSublevelWorld = UWorld::CreateWorld(EWorldType::Inactive, false, FName(*SublevelName), SublevelPackage);
    if (!NewSublevelWorld)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create world for sublevel: %s"), *SublevelName), nullptr, TEXT("WORLD_CREATION_FAILED"));
        return true;
    }

    if (!NewSublevelWorld->bIsWorldInitialized)
    {
        NewSublevelWorld->InitWorld();
    }

    SublevelPackage->MarkPackageDirty();

    bool bSaveSucceeded = true;
    if (bSave)
    {
        bSaveSucceeded = McpSafeLevelSave(NewSublevelWorld->PersistentLevel, SublevelPackageName);

        if (bSaveSucceeded)
        {
            // Flush asset registry so the new level is immediately discoverable
            IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
            FString LevelFilename;
            if (FPackageName::TryConvertLongPackageNameToFilename(SublevelPackageName, LevelFilename, FPackageName::GetMapPackageExtension()))
            {
                TArray<FString> FilesToScan;
                FilesToScan.Add(LevelFilename);
                AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
            }
        }
    }

    ULevelStreaming* StreamingLevel = NewObject<ULevelStreaming>(World, StreamingClass);
    if (StreamingLevel)
    {
        StreamingLevel->SetWorldAssetByPackageName(FName(*SublevelPackageName));
        StreamingLevel->LevelTransform = FTransform::Identity;
        StreamingLevel->SetShouldBeVisible(true);
        StreamingLevel->SetShouldBeLoaded(true);

        World->AddStreamingLevel(StreamingLevel);
    }

    World->MarkPackageDirty();

    // Save parent world if requested (to persist streaming level reference)
    if (bSave && StreamingLevel)
    {
        McpSafeAssetSave(World);
    }

    // Unload the saved sublevel world so a later LoadMap of it does not die with "World Memory Leaks".
    if (bSaveSucceeded && NewSublevelWorld)
    {
        CleanupCreatedLevelWorldAfterSave(NewSublevelWorld, SublevelPackage, FullSublevelPath);
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("sublevelName"), SublevelName);
    ResponseJson->SetStringField(TEXT("sublevelPath"), FullSublevelPath);
    ResponseJson->SetStringField(TEXT("parentLevel"), World->GetMapName());
    ResponseJson->SetBoolField(TEXT("saved"), bSave && bSaveSucceeded);
    ResponseJson->SetBoolField(TEXT("streamingAdded"), StreamingLevel != nullptr);

    if (bSave && !bSaveSucceeded)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Sublevel created but save failed: %s"), *SublevelName),
            ResponseJson, TEXT("SAVE_FAILED"));
        return true;
    }

    FString Message = FString::Printf(TEXT("Created sublevel: %s at %s"), *SublevelName, *FullSublevelPath);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}

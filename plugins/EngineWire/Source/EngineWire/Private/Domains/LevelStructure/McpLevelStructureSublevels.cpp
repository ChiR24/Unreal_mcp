#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "Components/ActorComponent.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

namespace McpLevelStructure
{

bool HandleCreateSublevel(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    // CRITICAL: sublevelName is required - no default fallback to prevent hidden errors
    FString SublevelName;
    Payload->TryGetStringField(TEXT("sublevelName"), SublevelName);

    if (SublevelName.IsEmpty())
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("sublevelName is required for create_sublevel"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FText InvalidNameReason;
    if (!IsValidLevelName(SublevelName, InvalidNameReason))
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid sublevelName '%s': %s"), *SublevelName, *InvalidNameReason.ToString()),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SublevelPath = GetJsonStringField(Payload, TEXT("sublevelPath"), TEXT(""));
    FString ParentLevel = GetJsonStringField(Payload, TEXT("parentLevel"), TEXT(""));
    // A new sublevel exists only once saved: with save false the new world was cleaned out of memory and the open level
    // was left streaming a package that exists nowhere.
    if (!GetJsonBoolField(Payload, TEXT("save"), true))
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("A new sublevel exists only once it is saved, so save false cannot make one. Nothing was created."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // streamingMethod picks the streaming class; it used to be ignored and every sublevel streamed by Blueprint.
    const FString StreamingMethod = GetJsonStringField(Payload, TEXT("streamingMethod"), TEXT("Blueprint"));
    UClass* StreamingClass = ResolveLevelStreamingClass(StreamingMethod);
    if (!StreamingClass)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown streamingMethod '%s'; use Blueprint or AlwaysLoaded."), *StreamingMethod), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("No editor world available"), TEXT("NO_EDITOR_WORLD"));
        return true;
    }

    // The sublevel always streams into the open level: a parentLevel naming another one was checked to exist and then
    // ignored, so the call reported the open level as the parent of a level it was never asked to change.
    const FString OpenLevel = World->GetOutermost()->GetName();
    if (!ParentLevel.IsEmpty())
    {
        FString NormalizedParentPath = FPackageName::ObjectPathToPackageName(ParentLevel);
        NormalizedParentPath = McpRootedContentPath(NormalizedParentPath);
        if (!NormalizedParentPath.Equals(OpenLevel, ESearchCase::IgnoreCase))
        {
            Subsystem->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("parentLevel %s is not the level open in the editor (%s); open it first (control_editor "
                    "open_level), then create the sublevel. Nothing was created."), *NormalizedParentPath, *OpenLevel),
                TEXT("PARENT_LEVEL_NOT_LOADED"));
            return true;
        }
    }

    FString FullSublevelPath;
    if (SublevelPath.IsEmpty())
    {
        // Beside the open level; a level never saved lives under /Temp, which made "/Game//Temp/<name>".
        FullSublevelPath = (OpenLevel.StartsWith(TEXT("/Game/")) ? FPaths::GetPath(OpenLevel) : FString(TEXT("/Game/Maps"))) / SublevelName;
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

    // The safe level save takes /Game alone, so a sublevel elsewhere is refused before anything is made. '/Game/' used to
    // be put in front of any other root, which made '/Game//Engine/...' of a path under /Engine or a plugin mount: a
    // name CreatePackage is a Fatal log for, as it is for a sublevelName holding two slashes.
    if (!FullSublevelPath.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(FullSublevelPath))
    {
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("A sublevel is created under /Game: %s is not a level "
            "path there. Nothing was created."), *FullSublevelPath), TEXT("INVALID_ARGUMENT"));
        return true;
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

        // Nothing is created or added here, so the reply says whether the open level streams it already.
        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            ExistingStreamingLevel
                ? FString::Printf(TEXT("Sublevel already exists and streams in the open level: %s"), *FullSublevelPath)
                : FString::Printf(TEXT("Sublevel already exists: %s; the open level does not stream it (manage_level add_sublevel "
                      "adds it). Nothing was created."), *FullSublevelPath),
            ResponseJson);
        return true;
    }

    FString SublevelPackageName = FullSublevelPath;

    UPackage* SublevelPackage = nullptr;
    UWorld* NewSublevelWorld = CreateLevelWorld(SublevelPackageName, SublevelName, SublevelPackage);
    if (!NewSublevelWorld)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            SublevelPackage ? FString::Printf(TEXT("Failed to create world for sublevel: %s"), *SublevelName)
                            : FString::Printf(TEXT("Failed to create package for sublevel: %s"), *SublevelPackageName),
            SublevelPackage ? TEXT("WORLD_CREATION_FAILED") : TEXT("PACKAGE_CREATION_FAILED"));
        return true;
    }

    SublevelPackage->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("sublevelName"), SublevelName);
    ResponseJson->SetStringField(TEXT("sublevelPath"), FullSublevelPath);
    ResponseJson->SetStringField(TEXT("parentLevel"), World->GetMapName());
    // The open level streams the sublevel only once it is on disk: a failed save used to add the entry and save the
    // open level with it before SAVE_FAILED was answered.
    if (!SaveLevelWorld(NewSublevelWorld, SublevelPackageName))
    {
        ResponseJson->SetBoolField(TEXT("saved"), false);
        ResponseJson->SetBoolField(TEXT("streamingAdded"), false);
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Sublevel created but save failed: %s; the open level does not stream it"), *SublevelName),
            ResponseJson, TEXT("SAVE_FAILED"));
        return true;
    }

    ULevelStreaming* StreamingLevel = NewObject<ULevelStreaming>(World, StreamingClass);
    StreamingLevel->SetWorldAssetByPackageName(FName(*SublevelPackageName));
    StreamingLevel->LevelTransform = FTransform::Identity;
    StreamingLevel->SetShouldBeVisible(true);
    StreamingLevel->SetShouldBeLoaded(true);
    World->AddStreamingLevel(StreamingLevel);
    World->MarkPackageDirty();
    // The open level keeps the streaming entry only once saved with it.
    ResponseJson->SetBoolField(TEXT("parentSaved"), McpSafeAssetSave(World));

    // Unload the saved sublevel world so a later LoadMap of it does not die with "World Memory Leaks".
    CleanupCreatedLevelWorldAfterSave(NewSublevelWorld, SublevelPackage, FullSublevelPath);

    ResponseJson->SetBoolField(TEXT("saved"), true);
    ResponseJson->SetBoolField(TEXT("streamingAdded"), true);
    // Stated: a level package is not among the assets the change journal hands the receipt, so its changes were empty. An
    // open level never saved (under /Temp) is no asset to name.
    TArray<FString> Changed = {FullSublevelPath};
    if (!OpenLevel.StartsWith(TEXT("/Temp/")))
    {
        Changed.Add(OpenLevel);
    }
    ResponseJson->SetArrayField(TEXT("changedAssets"), McpHandlerUtils::ToJsonStringArray(Changed));

    FString Message = FString::Printf(TEXT("Created sublevel: %s at %s"), *SublevelName, *FullSublevelPath);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}

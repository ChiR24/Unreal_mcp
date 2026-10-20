#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpLightingPrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Safety/McpSafeOperationsMapLoad.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace McpLightingHandlers
{

bool HandleCreateLightingEnabledLevel(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString Path;
    if (!Payload->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("path required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        Subsystem.SendAutomationError(
            RequestingSocket,
            RequestId,
            McpPathRefusalMessage(TEXT("path"), Path),
            TEXT("INVALID_PATH"));
        return true;
    }
    Path = SanitizedPath;

    // `path` is the level package path. When the caller also passes
    // `levelName` and `path` is a folder (its last segment differs), append the
    // name; otherwise a folder path was saved as a level named after the
    // folder (e.g. "/Game/MCPTest/Maps" -> level "Maps").
    FString LevelName;
    Payload->TryGetStringField(TEXT("name"), LevelName);
    if (LevelName.TrimStartAndEnd().IsEmpty())
    {
        Payload->TryGetStringField(TEXT("levelName"), LevelName);
    }
    LevelName.TrimStartAndEndInline();
    if (!LevelName.IsEmpty() && !FPaths::GetBaseFilename(Path).Equals(LevelName, ESearchCase::IgnoreCase))
    {
        Path = Path / LevelName;
    }

    FString LevelFilename;
    const bool bHasLevelFilename =
        FPackageName::TryConvertLongPackageNameToFilename(Path, LevelFilename, FPackageName::GetMapPackageExtension());
    const bool bLevelExistsOnDisk = bHasLevelFilename &&
        IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(LevelFilename));
    const bool bLevelExistsInRegistry = FPackageName::DoesPackageExist(Path);
    if (bLevelExistsOnDisk || bLevelExistsInRegistry)
    {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("path"), Path);
        Resp->SetBoolField(TEXT("alreadyExisted"), true);
        Resp->SetBoolField(TEXT("opened"), false);
        Resp->SetBoolField(TEXT("existsAfter"), true);
        Resp->SetStringField(TEXT("levelPath"), Path);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("A level already exists at %s; nothing was created or opened"), *Path), Resp);
        return true;
    }

    if (!GEditor)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }
    if (GEditor->PlayWorld)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Play In Editor is running; stop it first. Nothing was created."), TEXT("PIE_ACTIVE"));
        return true;
    }
    // NewMap replaces the open level like a load does, and dropped its unsaved changes without a word.
    TSharedPtr<FJsonObject> LossDetails;
    const FString Loss = McpSafeOperations::McpRefuseLoadOverUnsavedLevels(Path, TEXT("Save them with manage_level save first."), LossDetails);
    if (!Loss.IsEmpty())
    {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, Loss, LossDetails, TEXT("DIRTY_PACKAGES"));
        return true;
    }
    if (bHasLevelFilename)
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(LevelFilename), true);
    }

    // A sun driving a sky atmosphere, and a sky light capturing that sky in real time: a sky light alone captured an
    // empty scene and lit nothing.
    GEditor->NewMap();
    if (ADirectionalLight* Sun = SpawnActorInActiveWorld<ADirectionalLight>(
            ADirectionalLight::StaticClass(), FVector(0, 0, 500), FRotator(-45, 0, 0), TEXT("Sun")))
    {
        if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
        {
            SunLight->SetAtmosphereSunLight(true);
        }
    }
    SpawnActorInActiveWorld<AActor>(FindObject<UClass>(nullptr, TEXT("/Script/Engine.SkyAtmosphere")), FVector::ZeroVector, FRotator::ZeroRotator, TEXT("SkyAtmosphere"));
    if (ASkyLight* Sky = SpawnActorInActiveWorld<ASkyLight>(ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, TEXT("SkyLight")))
    {
        Sky->GetLightComponent()->bRealTimeCapture = true;
        Sky->GetLightComponent()->MarkRenderStateDirty();
    }

    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    const bool bSaved = EditorWorld && EditorWorld->PersistentLevel &&
        McpSafeLevelSave(EditorWorld->PersistentLevel, Path, 5);
    if (!bSaved)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save level"), TEXT("SAVE_FAILED"));
        return true;
    }

    if (bHasLevelFilename)
    {
        IAssetRegistry& AssetRegistry =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
        TArray<FString> FilesToScan;
        FilesToScan.Add(LevelFilename);
        AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), Path);
    Resp->SetStringField(TEXT("message"), TEXT("Level created with lighting"));
    Resp->SetBoolField(TEXT("existsAfter"), true);
    Resp->SetBoolField(TEXT("opened"), true);
    Resp->SetStringField(TEXT("levelPath"), Path);
    Resp->SetStringField(TEXT("createdAssetPath"), Path);
    TArray<TSharedPtr<FJsonValue>> Spawned;
    for (const TCHAR* Label : {TEXT("Sun"), TEXT("SkyAtmosphere"), TEXT("SkyLight")})
    {
        Spawned.Add(MakeShared<FJsonValueString>(Label));
    }
    Resp->SetArrayField(TEXT("actors"), Spawned);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level created with lighting"), Resp);
    return true;
}

}

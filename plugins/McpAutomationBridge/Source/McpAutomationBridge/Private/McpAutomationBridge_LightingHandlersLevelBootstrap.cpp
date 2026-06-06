#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LightingHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
namespace McpLightingHandlers
{

bool HandleCreateLightingEnabledLevel(
    UMcpAutomationBridgeSubsystem& Subsystem,
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
            TEXT("Invalid path: contains traversal or invalid characters"),
            TEXT("INVALID_PATH"));
        return true;
    }
    Path = SanitizedPath;

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
        Resp->SetBoolField(TEXT("existsAfter"), true);
        Resp->SetStringField(TEXT("levelPath"), Path);
        Subsystem.SendAutomationResponse(
            RequestingSocket,
            RequestId,
            true,
            FString::Printf(TEXT("Level already exists with lighting path: %s"), *Path),
            Resp);
        return true;
    }

    if (bHasLevelFilename)
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(LevelFilename), true);
    }

    if (!GEditor)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    GEditor->NewMap();
    SpawnActorInActiveWorld<AActor>(
        ADirectionalLight::StaticClass(), FVector(0, 0, 500), FRotator(-45, 0, 0), TEXT("Sun"));
    SpawnActorInActiveWorld<AActor>(
        ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, TEXT("SkyLight"));

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
    Resp->SetStringField(TEXT("levelPath"), Path);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level created with lighting"), Resp);
    return true;
}

}
#endif

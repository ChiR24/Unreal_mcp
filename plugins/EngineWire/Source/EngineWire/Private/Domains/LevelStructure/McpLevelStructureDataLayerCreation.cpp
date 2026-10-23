#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#endif
#include "WorldPartition/WorldPartition.h"

namespace McpLevelStructure
{

UDataLayerEditorSubsystem* RequireDataLayerWorld(UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const TCHAR* Operation, UWorld*& OutWorld)
{
    OutWorld = LevelStructureHelpers::GetEditorWorld();
    if (!OutWorld)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_EDITOR_WORLD"));
        return nullptr;
    }
    if (!OutWorld->GetWorldPartition())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition is not enabled for this level. Data layers require World Partition."), nullptr, TEXT("WORLD_PARTITION_NOT_ENABLED"));
        return nullptr;
    }
    // Without external objects (One File Per Actor) AddDataLayerInstance asserts
    // "GetLevel()->IsUsingExternalObjects()" (WorldDataLayers.cpp), and non-external actors cannot join a layer.
    ULevel* PersistentLevel = OutWorld->PersistentLevel;
    if (!PersistentLevel || !PersistentLevel->IsUsingExternalObjects())
    {
        TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
        ErrorDetails->SetStringField(TEXT("reason"), TEXT("One File Per Actor (OFPA) / External Actors is not enabled for this level."));
        ErrorDetails->SetStringField(TEXT("solution"), TEXT("Enable 'Use External Actors' in World Partition settings or convert the level via Edit > Convert Level."));
        ErrorDetails->SetBoolField(TEXT("worldPartitionEnabled"), true);
        ErrorDetails->SetBoolField(TEXT("externalActorsEnabled"), false);
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("%s requires 'One File Per Actor' (External Actors). Enable it in World Partition settings or use 'Edit > Convert Level' in the editor."), Operation),
            ErrorDetails, TEXT("EXTERNAL_ACTORS_NOT_ENABLED"));
        return nullptr;
    }
    UDataLayerEditorSubsystem* DataLayerEditorSubsystem = UDataLayerEditorSubsystem::Get();
    if (!DataLayerEditorSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Data Layer Editor Subsystem not available"), nullptr, TEXT("SUBSYSTEM_NOT_AVAILABLE"));
    }
    return DataLayerEditorSubsystem;
}

bool HandleCreateDataLayer(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if ENGINE_MINOR_VERSION >= 1
    using namespace LevelStructureHelpers;

    // CRITICAL: dataLayerName is required - no default fallback
    FString DataLayerName;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("dataLayerName"), DataLayerName);
    }

    if (DataLayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("dataLayerName is required for create_data_layer"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString DataLayerAssetPath = GetJsonStringField(Payload, TEXT("dataLayerAssetPath"), TEXT("/Game/DataLayers"));
    bool bIsInitiallyVisible = GetJsonBoolField(Payload, TEXT("bIsInitiallyVisible"), true);
    bool bIsInitiallyLoaded = GetJsonBoolField(Payload, TEXT("bIsInitiallyLoaded"), true);
    FString DataLayerType = GetJsonStringField(Payload, TEXT("dataLayerType"), TEXT("Runtime"));
    bool bIsPrivate = GetJsonBoolField(Payload, TEXT("bIsPrivate"), false);
    // Anything but an exact "Runtime" (even "runtime") made an Editor layer, which never streams in the game.
    const bool bRuntime = DataLayerType.Equals(TEXT("Runtime"), ESearchCase::IgnoreCase);
    if (!bRuntime && !DataLayerType.Equals(TEXT("Editor"), ESearchCase::IgnoreCase))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("dataLayerType '%s' is not one: use Runtime "
            "(streams in the game) or Editor (organises the editor only). Nothing was created."), *DataLayerType), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
#if !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    if (bIsPrivate)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("bIsPrivate needs UE 5.3 or later; omit it on this engine."), nullptr, TEXT("UNSUPPORTED_VERSION"));
        return true;
    }
#endif

    UWorld* World = nullptr;
    UDataLayerEditorSubsystem* DataLayerEditorSubsystem =
        RequireDataLayerWorld(Subsystem, RequestId, Socket, TEXT("Data layer creation"), World);
    if (!DataLayerEditorSubsystem)
    {
        return true;
    }

    // Security: Validate data layer asset path format to prevent traversal attacks
    FString SafeAssetPath = SanitizeProjectRelativePath(DataLayerAssetPath);
    if (SafeAssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            McpPathRefusalMessage(TEXT("data layer asset path"), DataLayerAssetPath),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }
    DataLayerAssetPath = SafeAssetPath;

    // Step 1: Create a UDataLayerAsset (the asset that backs the data layer instance)
    FString FullAssetPath = DataLayerAssetPath / DataLayerName;
    if (!FullAssetPath.StartsWith(TEXT("/Game/")))
    {
        FullAssetPath = TEXT("/Game/") + FullAssetPath;
    }

    FString CreateError, CreateCode;
    UPackage* AssetPackage = McpCreatePackageForNewAsset(FullAssetPath, CreateError, CreateCode);
    if (!AssetPackage)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            CreateError, nullptr, CreateCode);
        return true;
    }

    UDataLayerAsset* NewDataLayerAsset = NewObject<UDataLayerAsset>(AssetPackage, *DataLayerName, RF_Public | RF_Standalone);
    if (!NewDataLayerAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create UDataLayerAsset object"), nullptr, TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    NewDataLayerAsset->SetType(bRuntime ? EDataLayerType::Runtime : EDataLayerType::Editor);

    AssetPackage->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewDataLayerAsset);

    McpSafeAssetSave(NewDataLayerAsset);

    // Step 2: Create a UDataLayerInstance using the asset
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
    // 5.1 does not export FDataLayerCreationParameters (DATALAYEREDITOR_API arrives in 5.2), so its out-of-line
    // constructor cannot be called from this module. The struct is made through its reflection instead, which runs
    // that constructor inside DataLayerEditor.
    const UScriptStruct* CreationParamsType = FindObject<UScriptStruct>(nullptr, TEXT("/Script/DataLayerEditor.DataLayerCreationParameters"));
    if (!CreationParamsType)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Created DataLayerAsset '%s' but the engine has no DataLayerCreationParameters type to create its "
                "DataLayerInstance with. The asset exists at: %s"), *DataLayerName, *FullAssetPath), nullptr, TEXT("DATA_LAYER_INSTANCE_FAILED"));
        return true;
    }
    FStructOnScope CreationParamsScope(CreationParamsType);
    FDataLayerCreationParameters& CreationParams = *reinterpret_cast<FDataLayerCreationParameters*>(CreationParamsScope.GetStructMemory());
#else
    FDataLayerCreationParameters CreationParams;
#endif
    CreationParams.DataLayerAsset = NewDataLayerAsset;
    CreationParams.WorldDataLayers = World->GetWorldDataLayers();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    CreationParams.bIsPrivate = bIsPrivate;
#endif

    UDataLayerInstance* NewDataLayerInstance = DataLayerEditorSubsystem->CreateDataLayerInstance(CreationParams);
    if (!NewDataLayerInstance)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Created DataLayerAsset '%s' but failed to create DataLayerInstance. The asset exists at: %s"),
                *DataLayerName, *FullAssetPath), nullptr, TEXT("DATA_LAYER_INSTANCE_FAILED"));
        return true;
    }

    DataLayerEditorSubsystem->SetDataLayerVisibility(NewDataLayerInstance, bIsInitiallyVisible);
    DataLayerEditorSubsystem->SetDataLayerIsLoadedInEditor(NewDataLayerInstance, bIsInitiallyLoaded, false);

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, NewDataLayerAsset);
    ResponseJson->SetStringField(TEXT("dataLayerName"), DataLayerName);
    ResponseJson->SetStringField(TEXT("dataLayerAssetPath"), FullAssetPath);
    ResponseJson->SetStringField(TEXT("dataLayerType"), bRuntime ? TEXT("Runtime") : TEXT("Editor"));
    ResponseJson->SetBoolField(TEXT("initiallyVisible"), bIsInitiallyVisible);
    ResponseJson->SetBoolField(TEXT("initiallyLoaded"), bIsInitiallyLoaded);
    ResponseJson->SetBoolField(TEXT("isPrivate"), bIsPrivate);

    FString Message = FString::Printf(TEXT("Created data layer '%s' with asset at '%s'"),
        *DataLayerName, *FullAssetPath);
    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, World->PersistentLevel, Message, ResponseJson);
#else
    // UE 5.0 does not support the new DataLayer API
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("Data layer creation requires Unreal Engine 5.1 or later."), nullptr);
#endif
    return true;
}

}

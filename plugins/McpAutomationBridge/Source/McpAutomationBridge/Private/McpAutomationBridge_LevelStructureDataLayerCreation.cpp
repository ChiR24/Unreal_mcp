#include "McpAutomationBridge_LevelStructureActions.h"
#include "McpAutomationBridge_LevelStructureEditorWorld.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#endif
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleCreateDataLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
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

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_EDITOR_WORLD"));
        return true;
    }

    // Check if World Partition is enabled
    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition is not enabled for this level. Data layers require World Partition."), nullptr, TEXT("WORLD_PARTITION_NOT_ENABLED"));
        return true;
    }

    // CRITICAL: Check if the level uses External Objects (One File Per Actor / OFPA)
    // Data Layer instances require OFPA to be enabled, otherwise AddDataLayerInstance()
    // will hit an assertion: "GetLevel()->IsUsingExternalObjects()"
    // See WorldDataLayers.cpp:685
    ULevel* PersistentLevel = World->PersistentLevel;
    if (!PersistentLevel || !PersistentLevel->IsUsingExternalObjects())
    {
        TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
        ErrorDetails->SetStringField(TEXT("reason"), TEXT("One File Per Actor (OFPA) / External Actors is not enabled for this level."));
        ErrorDetails->SetStringField(TEXT("solution"), TEXT("Enable 'Use External Actors' in World Partition settings or convert the level via Edit > Convert Level."));
        ErrorDetails->SetBoolField(TEXT("worldPartitionEnabled"), true);
        ErrorDetails->SetBoolField(TEXT("externalActorsEnabled"), PersistentLevel ? PersistentLevel->IsUsingExternalObjects() : false);

        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Data layers require 'One File Per Actor' (External Actors) to be enabled. Enable it in World Partition settings or use 'Edit > Convert Level' in the editor."),
            ErrorDetails, TEXT("EXTERNAL_ACTORS_NOT_ENABLED"));
        return true;
    }

    // Get the Data Layer Editor Subsystem
    UDataLayerEditorSubsystem* DataLayerEditorSubsystem = UDataLayerEditorSubsystem::Get();
    if (!DataLayerEditorSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Data Layer Editor Subsystem not available"), nullptr, TEXT("SUBSYSTEM_NOT_AVAILABLE"));
        return true;
    }

    // Security: Validate data layer asset path format to prevent traversal attacks
    FString SafeAssetPath = SanitizeProjectRelativePath(DataLayerAssetPath);
    if (SafeAssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid or unsafe data layer asset path: %s"), *DataLayerAssetPath),
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

    // Create the package for the data layer asset
    UPackage* AssetPackage = CreatePackage(*FullAssetPath);
    if (!AssetPackage)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create package for DataLayerAsset at: %s"), *FullAssetPath), nullptr, TEXT("PACKAGE_CREATION_FAILED"));
        return true;
    }

    // Create the UDataLayerAsset
    UDataLayerAsset* NewDataLayerAsset = NewObject<UDataLayerAsset>(AssetPackage, *DataLayerName, RF_Public | RF_Standalone);
    if (!NewDataLayerAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create UDataLayerAsset object"), nullptr, TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    // Configure the data layer asset type
    if (DataLayerType == TEXT("Runtime"))
    {
        NewDataLayerAsset->SetType(EDataLayerType::Runtime);
    }
    else
    {
        NewDataLayerAsset->SetType(EDataLayerType::Editor);
    }

    // Mark package dirty and notify asset registry
    AssetPackage->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewDataLayerAsset);

    // Save the asset
    McpSafeAssetSave(NewDataLayerAsset);

    // Step 2: Create a UDataLayerInstance using the asset
    FDataLayerCreationParameters CreationParams;
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
                *DataLayerName, *FullAssetPath), nullptr);
        return true;
    }

    // Configure initial visibility and loaded state
    DataLayerEditorSubsystem->SetDataLayerVisibility(NewDataLayerInstance, bIsInitiallyVisible);
    DataLayerEditorSubsystem->SetDataLayerIsLoadedInEditor(NewDataLayerInstance, bIsInitiallyLoaded, false);

    // Mark world dirty
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, NewDataLayerAsset);
    ResponseJson->SetStringField(TEXT("dataLayerName"), DataLayerName);
    ResponseJson->SetStringField(TEXT("dataLayerAssetPath"), FullAssetPath);
    ResponseJson->SetStringField(TEXT("dataLayerType"), DataLayerType);
    ResponseJson->SetBoolField(TEXT("initiallyVisible"), bIsInitiallyVisible);
    ResponseJson->SetBoolField(TEXT("initiallyLoaded"), bIsInitiallyLoaded);
    ResponseJson->SetBoolField(TEXT("isPrivate"), bIsPrivate);

    FString Message = FString::Printf(TEXT("Created data layer '%s' with asset at '%s'"),
        *DataLayerName, *FullAssetPath);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
#else
    // UE 5.0 does not support the new DataLayer API
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("Data layer creation requires Unreal Engine 5.1 or later."), nullptr);
#endif
    return true;
}

}
#endif

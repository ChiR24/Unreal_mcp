#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Domains/Inventory/McpGenericDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/DataAssetFactory.h"
#include "EditorAssetLibrary.h"


// The package for a new inventory asset Name in folder Path (/Game/Items when empty); null after replying
// PACKAGE_CREATE_FAILED for a path outside the allowed roots, ASSET_ALREADY_EXISTS for an occupied one.
UPackage* CreateInventoryAssetPackage(UEngineWireSubsystem& Bridge, const FString& RequestId,
                                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString& Path,
                                      const FString& Name);

// A new generic data asset (UMcpGenericDataAsset: UDataAsset and UPrimaryDataAsset are abstract) named Name in Package.
// Fill writes its first values; then it is dirtied, registered and, unless the payload's save is false, saved (OutSaved).
// Null when NewObject makes nothing.
UMcpGenericDataAsset* NewInventoryDataAsset(UPackage* Package, const FString& Name, const TSharedPtr<FJsonObject>& Payload, bool& OutSaved,
                                            TFunctionRef<void(UMcpGenericDataAsset&)> Fill = [](UMcpGenericDataAsset&) {});

// create_item_category and create_loot_table: a generic data asset named by the payload's name, in its path or
// DefaultPath, answered with saved, PathField and assetPath under Created; "Failed to create <What> asset" when none was
// made, the name and package refusals as CreateInventoryAssetPackage gives them. True: the request is answered.
bool CreateGenericInventoryAsset(UEngineWireSubsystem& Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                 const TSharedPtr<FJsonObject>& Payload, const TCHAR* DefaultPath, const TCHAR* PathField,
                                 const TCHAR* Created, const TCHAR* What);

// False after replying ASSET_NOT_FOUND when Path, a reference the action stores (item, category, icon,
// ingredient), names no asset. Those paths used to be stored unchecked, so a typo saved a dead reference.
bool RequireReferencedAsset(UEngineWireSubsystem& Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                            const FString& Path, const TCHAR* Field);

// The number after the highest "<Prefix><n>" key in Properties (0 when none). Properties.Num() is not
// that number once a key has been removed, and using it as the next key overwrote a surviving entry.
int32 NextIndexedPropertyIndex(const TMap<FString, FString>& Properties, const FString& Prefix);

// Twelve inventory actions opened with the same seventeen lines: read
// blueprintPath, refuse when it is absent, load the Blueprint, refuse when it
// does not resolve. Changing either refusal meant editing all twelve.
UBlueprint* LoadInventoryBlueprintOrError(UEngineWireSubsystem& Bridge,
                                          const FString& RequestId,
                                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                          const FString& BlueprintPath);

// The data asset at Path as a T, or null after refusing: MISSING_PARAMETER "Missing required parameter: <Field>" for an
// empty Path, ASSET_NOT_FOUND "<Noun> not found: <Path>" when nothing there loads as a T. It loads through UDataAsset,
// since UPrimaryDataAsset is abstract on 5.7.
template <typename T = UObject>
T* LoadInventoryAssetOrError(UEngineWireSubsystem& Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                             const FString& Path, const TCHAR* Field, const TCHAR* Noun)
{
  if (Path.IsEmpty()) {
    Bridge.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Missing required parameter: %s"), Field),
                               TEXT("MISSING_PARAMETER"));
    return nullptr;
  }
  T* Asset = Cast<T>(StaticLoadObject(UDataAsset::StaticClass(), nullptr, *Path));
  if (!Asset) {
    Bridge.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s not found: %s"), Noun, *Path),
                               TEXT("ASSET_NOT_FOUND"));
  }
  return Asset;
}

bool HandleInventoryDataAssetActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInventoryCategoryActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInventoryReplicationActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInventoryLootTableActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInventoryLootDropActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInventoryCraftingRecipeActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInventoryCraftingStationActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInventoryItemPresentationActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInventoryInfoActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

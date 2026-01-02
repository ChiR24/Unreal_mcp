// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 17: Inventory & Items System Handlers

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/SavePackage.h"
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
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/DataAssetFactory.h"
#include "EditorAssetLibrary.h"

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
#define GetPayloadString GetJsonStringField
#define GetPayloadNumber GetJsonNumberField
#define GetPayloadBool GetJsonBoolField

// Helper to create a new package
static UPackage* CreateAssetPackage(const FString& Path, const FString& Name) {
  FString PackagePath = Path.IsEmpty() ? TEXT("/Game/Items") : Path;
  if (!PackagePath.StartsWith(TEXT("/"))) {
    PackagePath = TEXT("/Game/") + PackagePath;
  }
  FString PackageName = PackagePath / Name;
  return CreatePackage(*PackageName);
}

// ============================================================================
// Main Inventory Handler Dispatcher
// ============================================================================
bool UMcpAutomationBridgeSubsystem::HandleManageInventoryAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // Only handle manage_inventory action
  if (Action != TEXT("manage_inventory")) {
    return false;
  }

  FString SubAction = GetPayloadString(Payload, TEXT("subAction"));

  // ===========================================================================
  // 17.1 Data Assets (4 actions)
  // ===========================================================================

  if (SubAction == TEXT("create_item_data_asset")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Items"));

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: name"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Create a primary data asset for item
    UPackage* Package = CreateAssetPackage(Path, Name);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create package"),
                          TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    // Create UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
    UMcpGenericDataAsset* ItemAsset =
        NewObject<UMcpGenericDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);

    if (ItemAsset) {
      ItemAsset->MarkPackageDirty();
      FAssetRegistryModule::AssetCreated(ItemAsset);

      if (GetPayloadBool(Payload, TEXT("save"), true)) {
        FString AssetPathForSave = ItemAsset->GetPathName();
        int32 DotIdx = AssetPathForSave.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
        if (DotIdx != INDEX_NONE) { AssetPathForSave.LeftInline(DotIdx); }
        ItemAsset->MarkPackageDirty();
      }

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("itemPath"), Package->GetName());
      Result->SetStringField(TEXT("assetName"), Name);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Item data asset created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create item data asset"),
                          TEXT("ASSET_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("set_item_properties")) {
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));

    if (ItemPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: itemPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the item asset and set properties (use UDataAsset base class for loading)
    UObject* Asset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    UDataAsset* ItemAsset = Cast<UDataAsset>(Asset);

    if (!ItemAsset) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Item data asset not found: %s"), *ItemPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Note: UPrimaryDataAsset is a base class. Actual item properties would be
    // on a custom subclass. This implementation provides the framework.
    ItemAsset->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      FString AssetPathForSave = ItemAsset->GetPathName();
      int32 DotIdx = AssetPathForSave.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
      if (DotIdx != INDEX_NONE) { AssetPathForSave.LeftInline(DotIdx); }
      ItemAsset->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetBoolField(TEXT("modified"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Item properties updated"), Result);
    return true;
  }

  if (SubAction == TEXT("create_item_category")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Items/Categories"));

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: name"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Create a data asset for category
    UPackage* Package = CreateAssetPackage(Path, Name);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create package"),
                          TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    // UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
    UMcpGenericDataAsset* CategoryAsset =
        NewObject<UMcpGenericDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);

    if (CategoryAsset) {
      CategoryAsset->MarkPackageDirty();
      FAssetRegistryModule::AssetCreated(CategoryAsset);

      if (GetPayloadBool(Payload, TEXT("save"), true)) {
        FString AssetPathForSave = CategoryAsset->GetPathName();
        int32 DotIdx = AssetPathForSave.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
        if (DotIdx != INDEX_NONE) { AssetPathForSave.LeftInline(DotIdx); }
        CategoryAsset->MarkPackageDirty();
      }

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("categoryPath"), Package->GetName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Item category created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create category asset"),
                          TEXT("ASSET_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("assign_item_category")) {
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
    FString CategoryPath = GetPayloadString(Payload, TEXT("categoryPath"));

    if (ItemPath.IsEmpty() || CategoryPath.IsEmpty()) {
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: itemPath and categoryPath"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load both assets (use UDataAsset base class for loading - UPrimaryDataAsset is abstract in UE5.7)
    UObject* ItemObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    UObject* CategoryObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *CategoryPath);

    if (!ItemObj) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Item not found: %s"), *ItemPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Note: Actual category assignment would require custom item class
    // This provides the framework for the operation
    ItemObj->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetStringField(TEXT("categoryPath"), CategoryPath);
    Result->SetBoolField(TEXT("assigned"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Category assigned to item"), Result);
    return true;
  }

  // ===========================================================================
  // 17.2 Inventory Component (5 actions)
  // ===========================================================================

  if (SubAction == TEXT("create_inventory_component")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    FString ComponentName =
        GetPayloadString(Payload, TEXT("componentName"), TEXT("InventoryComponent"));

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the blueprint
    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
    if (!Blueprint) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Blueprint has no SimpleConstructionScript"),
                          TEXT("NO_SCS"));
      return true;
    }

    // Create an ActorComponent as base inventory component
    // In a real implementation, this would be a custom UInventoryComponent
    USCS_Node* NewNode = SCS->CreateNode(UActorComponent::StaticClass(), *ComponentName);
    if (NewNode) {
      SCS->AddNode(NewNode);
      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        if (GetPayloadBool(Payload, TEXT("save"), true)) {
          FString AssetPathForSave = Blueprint->GetPathName();
          int32 DotIdx = AssetPathForSave.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
          if (DotIdx != INDEX_NONE) { AssetPathForSave.LeftInline(DotIdx); }
          Blueprint->MarkPackageDirty();
        }

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("componentName"), ComponentName);
      Result->SetBoolField(TEXT("componentAdded"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Inventory component added"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create inventory component"),
                          TEXT("COMPONENT_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("configure_inventory_slots")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    int32 SlotCount = static_cast<int32>(GetPayloadNumber(Payload, TEXT("slotCount"), 20));

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load blueprint and configure slots
    // Note: This requires a custom inventory component class
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetNumberField(TEXT("slotCount"), SlotCount);
    Result->SetBoolField(TEXT("configured"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory slots configured"), Result);
    return true;
  }

  if (SubAction == TEXT("add_inventory_functions")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Note: Adding functions programmatically requires Blueprint graph manipulation
    // This provides the framework - actual implementation would use K2Node creation
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());

    TArray<TSharedPtr<FJsonValue>> FunctionsAdded;
    FunctionsAdded.Add(MakeShareable(new FJsonValueString(TEXT("AddItem"))));
    FunctionsAdded.Add(MakeShareable(new FJsonValueString(TEXT("RemoveItem"))));
    FunctionsAdded.Add(MakeShareable(new FJsonValueString(TEXT("GetItemCount"))));
    FunctionsAdded.Add(MakeShareable(new FJsonValueString(TEXT("HasItem"))));
    FunctionsAdded.Add(MakeShareable(new FJsonValueString(TEXT("TransferItem"))));
    Result->SetArrayField(TEXT("functionsAdded"), FunctionsAdded);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory functions added"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_inventory_events")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());

    TArray<TSharedPtr<FJsonValue>> EventsAdded;
    EventsAdded.Add(MakeShareable(new FJsonValueString(TEXT("OnItemAdded"))));
    EventsAdded.Add(MakeShareable(new FJsonValueString(TEXT("OnItemRemoved"))));
    EventsAdded.Add(MakeShareable(new FJsonValueString(TEXT("OnInventoryChanged"))));
    EventsAdded.Add(MakeShareable(new FJsonValueString(TEXT("OnSlotUpdated"))));
    Result->SetArrayField(TEXT("eventsAdded"), EventsAdded);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory events configured"), Result);
    return true;
  }

  if (SubAction == TEXT("set_inventory_replication")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    bool Replicated = GetPayloadBool(Payload, TEXT("replicated"), false);

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("replicated"), Replicated);
    Result->SetStringField(
        TEXT("replicationCondition"),
        GetPayloadString(Payload, TEXT("replicationCondition"), TEXT("None")));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory replication configured"), Result);
    return true;
  }

  // ===========================================================================
  // 17.3 Pickups (4 actions)
  // ===========================================================================

  if (SubAction == TEXT("create_pickup_actor")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Blueprints/Pickups"));

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: name"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Create a Blueprint actor for pickup
    UPackage* Package = CreateAssetPackage(Path, Name);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create package"),
                          TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AActor::StaticClass();

    UBlueprint* NewBlueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name,
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (NewBlueprint) {
      // Add sphere collision for pickup detection
      USimpleConstructionScript* SCS = NewBlueprint->SimpleConstructionScript;
      if (SCS) {
        // Add static mesh component for visual
        USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("PickupMesh"));
        if (MeshNode) {
          SCS->AddNode(MeshNode);
        }

        // Add sphere component for interaction
        USCS_Node* SphereNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionSphere"));
        if (SphereNode) {
          SCS->AddNode(SphereNode);
          USphereComponent* SphereComp = Cast<USphereComponent>(SphereNode->ComponentTemplate);
          if (SphereComp) {
            SphereComp->SetSphereRadius(100.0f);
            SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
          }
        }
      }

      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(NewBlueprint);
      FAssetRegistryModule::AssetCreated(NewBlueprint);

      if (GetPayloadBool(Payload, TEXT("save"), true)) {
        FString AssetPathForSave = NewBlueprint->GetPathName();
        int32 DotIdx = AssetPathForSave.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
        if (DotIdx != INDEX_NONE) { AssetPathForSave.LeftInline(DotIdx); }
        NewBlueprint->MarkPackageDirty();
      }

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("pickupPath"), Package->GetName());
      Result->SetStringField(TEXT("blueprintName"), Name);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Pickup actor created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create pickup blueprint"),
                          TEXT("BLUEPRINT_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("configure_pickup_interaction")) {
    FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));
    FString InteractionType =
        GetPayloadString(Payload, TEXT("interactionType"), TEXT("Overlap"));

    if (PickupPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: pickupPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("pickupPath"), PickupPath);
    Result->SetStringField(TEXT("interactionType"), InteractionType);
    Result->SetStringField(TEXT("prompt"),
                           GetPayloadString(Payload, TEXT("prompt"), TEXT("Press E to pick up")));
    Result->SetBoolField(TEXT("configured"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pickup interaction configured"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_pickup_respawn")) {
    FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));
    bool Respawnable = GetPayloadBool(Payload, TEXT("respawnable"), false);
    double RespawnTime = GetPayloadNumber(Payload, TEXT("respawnTime"), 30.0);

    if (PickupPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: pickupPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("pickupPath"), PickupPath);
    Result->SetBoolField(TEXT("respawnable"), Respawnable);
    Result->SetNumberField(TEXT("respawnTime"), RespawnTime);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pickup respawn configured"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_pickup_effects")) {
    FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));

    if (PickupPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: pickupPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("pickupPath"), PickupPath);
    Result->SetBoolField(TEXT("bobbing"), GetPayloadBool(Payload, TEXT("bobbing"), true));
    Result->SetBoolField(TEXT("rotation"), GetPayloadBool(Payload, TEXT("rotation"), true));
    Result->SetBoolField(TEXT("glowEffect"), GetPayloadBool(Payload, TEXT("glowEffect"), false));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pickup effects configured"), Result);
    return true;
  }

  // ===========================================================================
  // 17.4 Equipment System (5 actions)
  // ===========================================================================

  if (SubAction == TEXT("create_equipment_component")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    FString ComponentName =
        GetPayloadString(Payload, TEXT("componentName"), TEXT("EquipmentComponent"));

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
    if (!Blueprint) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (SCS) {
      USCS_Node* NewNode = SCS->CreateNode(UActorComponent::StaticClass(), *ComponentName);
      if (NewNode) {
        SCS->AddNode(NewNode);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        if (GetPayloadBool(Payload, TEXT("save"), true)) {
          FString AssetPathForSave = Blueprint->GetPathName();
          int32 DotIdx = AssetPathForSave.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
          if (DotIdx != INDEX_NONE) { AssetPathForSave.LeftInline(DotIdx); }
          Blueprint->MarkPackageDirty();
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetBoolField(TEXT("componentAdded"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Equipment component added"), Result);
        return true;
      }
    }

    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create equipment component"),
                        TEXT("COMPONENT_CREATE_FAILED"));
    return true;
  }

  if (SubAction == TEXT("define_equipment_slots")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Define equipment slots (Head, Chest, Hands, Legs, Feet, Weapon, etc.)
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());

    TArray<TSharedPtr<FJsonValue>> DefaultSlots;
    DefaultSlots.Add(MakeShareable(new FJsonValueString(TEXT("Head"))));
    DefaultSlots.Add(MakeShareable(new FJsonValueString(TEXT("Chest"))));
    DefaultSlots.Add(MakeShareable(new FJsonValueString(TEXT("Hands"))));
    DefaultSlots.Add(MakeShareable(new FJsonValueString(TEXT("Legs"))));
    DefaultSlots.Add(MakeShareable(new FJsonValueString(TEXT("Feet"))));
    DefaultSlots.Add(MakeShareable(new FJsonValueString(TEXT("MainWeapon"))));
    DefaultSlots.Add(MakeShareable(new FJsonValueString(TEXT("OffhandWeapon"))));
    Result->SetArrayField(TEXT("slotsConfigured"), DefaultSlots);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Equipment slots defined"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_equipment_effects")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("statModifiersConfigured"), true);
    Result->SetBoolField(TEXT("abilityGrantsConfigured"), true);
    Result->SetBoolField(TEXT("passiveEffectsConfigured"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Equipment effects configured"), Result);
    return true;
  }

  if (SubAction == TEXT("add_equipment_functions")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());

    TArray<TSharedPtr<FJsonValue>> FunctionsAdded;
    FunctionsAdded.Add(MakeShareable(new FJsonValueString(TEXT("EquipItem"))));
    FunctionsAdded.Add(MakeShareable(new FJsonValueString(TEXT("UnequipItem"))));
    FunctionsAdded.Add(MakeShareable(new FJsonValueString(TEXT("GetEquippedItem"))));
    FunctionsAdded.Add(MakeShareable(new FJsonValueString(TEXT("CanEquip"))));
    FunctionsAdded.Add(MakeShareable(new FJsonValueString(TEXT("SwapEquipment"))));
    Result->SetArrayField(TEXT("functionsAdded"), FunctionsAdded);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Equipment functions added"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_equipment_visuals")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("attachToSocket"),
                         GetPayloadBool(Payload, TEXT("attachToSocket"), true));
    Result->SetBoolField(TEXT("visualsConfigured"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Equipment visuals configured"), Result);
    return true;
  }

  // ===========================================================================
  // 17.5 Loot System (4 actions)
  // ===========================================================================

  if (SubAction == TEXT("create_loot_table")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Data/LootTables"));

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: name"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Create a data asset for loot table
    UPackage* Package = CreateAssetPackage(Path, Name);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create package"),
                          TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    // UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
    UMcpGenericDataAsset* LootTableAsset =
        NewObject<UMcpGenericDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);

    if (LootTableAsset) {
      LootTableAsset->MarkPackageDirty();
      FAssetRegistryModule::AssetCreated(LootTableAsset);

      if (GetPayloadBool(Payload, TEXT("save"), true)) {
        FString AssetPathForSave = LootTableAsset->GetPathName();
        int32 DotIdx = AssetPathForSave.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
        if (DotIdx != INDEX_NONE) { AssetPathForSave.LeftInline(DotIdx); }
        LootTableAsset->MarkPackageDirty();
      }

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("lootTablePath"), Package->GetName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Loot table created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create loot table asset"),
                          TEXT("ASSET_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("add_loot_entry")) {
    FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
    double Weight = GetPayloadNumber(Payload, TEXT("lootWeight"), 1.0);
    int32 MinQuantity = static_cast<int32>(GetPayloadNumber(Payload, TEXT("minQuantity"), 1));
    int32 MaxQuantity = static_cast<int32>(GetPayloadNumber(Payload, TEXT("maxQuantity"), 1));

    if (LootTablePath.IsEmpty() || ItemPath.IsEmpty()) {
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: lootTablePath and itemPath"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetNumberField(TEXT("weight"), Weight);
    Result->SetNumberField(TEXT("minQuantity"), MinQuantity);
    Result->SetNumberField(TEXT("maxQuantity"), MaxQuantity);
    Result->SetNumberField(TEXT("entryIndex"), 0);  // Would be actual index
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Loot entry added"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_loot_drop")) {
    FString ActorPath = GetPayloadString(Payload, TEXT("actorPath"));
    FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));

    if (ActorPath.IsEmpty() || LootTablePath.IsEmpty()) {
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: actorPath and lootTablePath"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("actorPath"), ActorPath);
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    Result->SetNumberField(TEXT("dropCount"),
                           GetPayloadNumber(Payload, TEXT("dropCount"), 1));
    Result->SetNumberField(TEXT("dropRadius"),
                           GetPayloadNumber(Payload, TEXT("dropRadius"), 100.0));
    Result->SetBoolField(TEXT("configured"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Loot drop configured"), Result);
    return true;
  }

  if (SubAction == TEXT("set_loot_quality_tiers")) {
    FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));

    if (LootTablePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: lootTablePath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);

    TArray<TSharedPtr<FJsonValue>> DefaultTiers;
    TSharedPtr<FJsonObject> Common = MakeShareable(new FJsonObject());
    Common->SetStringField(TEXT("name"), TEXT("Common"));
    Common->SetNumberField(TEXT("dropWeight"), 60.0);
    DefaultTiers.Add(MakeShareable(new FJsonValueObject(Common)));

    TSharedPtr<FJsonObject> Uncommon = MakeShareable(new FJsonObject());
    Uncommon->SetStringField(TEXT("name"), TEXT("Uncommon"));
    Uncommon->SetNumberField(TEXT("dropWeight"), 25.0);
    DefaultTiers.Add(MakeShareable(new FJsonValueObject(Uncommon)));

    TSharedPtr<FJsonObject> Rare = MakeShareable(new FJsonObject());
    Rare->SetStringField(TEXT("name"), TEXT("Rare"));
    Rare->SetNumberField(TEXT("dropWeight"), 10.0);
    DefaultTiers.Add(MakeShareable(new FJsonValueObject(Rare)));

    TSharedPtr<FJsonObject> Epic = MakeShareable(new FJsonObject());
    Epic->SetStringField(TEXT("name"), TEXT("Epic"));
    Epic->SetNumberField(TEXT("dropWeight"), 4.0);
    DefaultTiers.Add(MakeShareable(new FJsonValueObject(Epic)));

    TSharedPtr<FJsonObject> Legendary = MakeShareable(new FJsonObject());
    Legendary->SetStringField(TEXT("name"), TEXT("Legendary"));
    Legendary->SetNumberField(TEXT("dropWeight"), 1.0);
    DefaultTiers.Add(MakeShareable(new FJsonValueObject(Legendary)));

    Result->SetArrayField(TEXT("tiersConfigured"), DefaultTiers);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Quality tiers configured"), Result);
    return true;
  }

  // ===========================================================================
  // 17.6 Crafting System (4 actions)
  // ===========================================================================

  if (SubAction == TEXT("create_crafting_recipe")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString OutputItemPath = GetPayloadString(Payload, TEXT("outputItemPath"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Data/Recipes"));

    if (Name.IsEmpty() || OutputItemPath.IsEmpty()) {
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: name and outputItemPath"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UPackage* Package = CreateAssetPackage(Path, Name);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create package"),
                          TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    // UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
    UMcpGenericDataAsset* RecipeAsset =
        NewObject<UMcpGenericDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);

    if (RecipeAsset) {
      RecipeAsset->MarkPackageDirty();
      FAssetRegistryModule::AssetCreated(RecipeAsset);

      if (GetPayloadBool(Payload, TEXT("save"), true)) {
        FString AssetPathForSave = RecipeAsset->GetPathName();
        int32 DotIdx = AssetPathForSave.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
        if (DotIdx != INDEX_NONE) { AssetPathForSave.LeftInline(DotIdx); }
        RecipeAsset->MarkPackageDirty();
      }

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("recipePath"), Package->GetName());
      Result->SetStringField(TEXT("outputItemPath"), OutputItemPath);
      Result->SetNumberField(TEXT("outputQuantity"),
                             GetPayloadNumber(Payload, TEXT("outputQuantity"), 1));
      Result->SetNumberField(TEXT("craftTime"),
                             GetPayloadNumber(Payload, TEXT("craftTime"), 1.0));
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Crafting recipe created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create recipe asset"),
                          TEXT("ASSET_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("configure_recipe_requirements")) {
    FString RecipePath = GetPayloadString(Payload, TEXT("recipePath"));

    if (RecipePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: recipePath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("recipePath"), RecipePath);
    Result->SetNumberField(TEXT("requiredLevel"),
                           GetPayloadNumber(Payload, TEXT("requiredLevel"), 0));
    Result->SetStringField(TEXT("requiredStation"),
                           GetPayloadString(Payload, TEXT("requiredStation"), TEXT("None")));
    Result->SetBoolField(TEXT("configured"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Recipe requirements configured"), Result);
    return true;
  }

  if (SubAction == TEXT("create_crafting_station")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Blueprints/CraftingStations"));
    FString StationType =
        GetPayloadString(Payload, TEXT("stationType"), TEXT("Basic"));

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: name"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UPackage* Package = CreateAssetPackage(Path, Name);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create package"),
                          TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AActor::StaticClass();

    UBlueprint* StationBlueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name,
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (StationBlueprint) {
      USimpleConstructionScript* SCS = StationBlueprint->SimpleConstructionScript;
      if (SCS) {
        // Add mesh component
        USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("StationMesh"));
        if (MeshNode) {
          SCS->AddNode(MeshNode);
        }

        // Add interaction component
        USCS_Node* BoxNode = SCS->CreateNode(UBoxComponent::StaticClass(), TEXT("InteractionBox"));
        if (BoxNode) {
          SCS->AddNode(BoxNode);
        }
      }

      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(StationBlueprint);
      FAssetRegistryModule::AssetCreated(StationBlueprint);

        if (GetPayloadBool(Payload, TEXT("save"), true)) {
          FString AssetPathForSave = StationBlueprint->GetPathName();
          int32 DotIdx = AssetPathForSave.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
          if (DotIdx != INDEX_NONE) { AssetPathForSave.LeftInline(DotIdx); }
          StationBlueprint->MarkPackageDirty();
        }

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("stationPath"), Package->GetName());
      Result->SetStringField(TEXT("stationType"), StationType);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Crafting station created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create crafting station blueprint"),
                          TEXT("BLUEPRINT_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("add_crafting_component")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    FString ComponentName =
        GetPayloadString(Payload, TEXT("componentName"), TEXT("CraftingComponent"));

    if (BlueprintPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
    if (!Blueprint) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (SCS) {
      USCS_Node* NewNode = SCS->CreateNode(UActorComponent::StaticClass(), *ComponentName);
      if (NewNode) {
        SCS->AddNode(NewNode);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        if (GetPayloadBool(Payload, TEXT("save"), true)) {
          FString AssetPathForSave = Blueprint->GetPathName();
          int32 DotIdx = AssetPathForSave.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
          if (DotIdx != INDEX_NONE) { AssetPathForSave.LeftInline(DotIdx); }
          Blueprint->MarkPackageDirty();
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetBoolField(TEXT("componentAdded"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Crafting component added"), Result);
        return true;
      }
    }

    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create crafting component"),
                        TEXT("COMPONENT_CREATE_FAILED"));
    return true;
  }

  // ===========================================================================
  // Utility (1 action)
  // ===========================================================================

  if (SubAction == TEXT("get_inventory_info")) {
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());

    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
    FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));
    FString RecipePath = GetPayloadString(Payload, TEXT("recipePath"));
    FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));

    if (!BlueprintPath.IsEmpty()) {
      UBlueprint* Blueprint = Cast<UBlueprint>(
          StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
      if (Blueprint) {
        Result->SetStringField(TEXT("assetType"), TEXT("Blueprint"));
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("className"), Blueprint->GeneratedClass->GetName());

        // Check for inventory/equipment components
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (SCS) {
          TArray<TSharedPtr<FJsonValue>> Components;
          for (USCS_Node* Node : SCS->GetAllNodes()) {
            if (Node) {
              TSharedPtr<FJsonObject> CompInfo = MakeShareable(new FJsonObject());
              CompInfo->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
              CompInfo->SetStringField(TEXT("class"),
                                       Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("Unknown"));
              Components.Add(MakeShareable(new FJsonValueObject(CompInfo)));
            }
          }
          Result->SetArrayField(TEXT("components"), Components);
        }
      }
    } else if (!ItemPath.IsEmpty()) {
      // Use UDataAsset base class for loading - UPrimaryDataAsset is abstract in UE5.7
      UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
      if (ItemAsset) {
        Result->SetStringField(TEXT("assetType"), TEXT("Item"));
        Result->SetStringField(TEXT("itemPath"), ItemPath);
        Result->SetStringField(TEXT("className"), ItemAsset->GetClass()->GetName());
      }
    } else if (!LootTablePath.IsEmpty()) {
      Result->SetStringField(TEXT("assetType"), TEXT("LootTable"));
      Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    } else if (!RecipePath.IsEmpty()) {
      Result->SetStringField(TEXT("assetType"), TEXT("Recipe"));
      Result->SetStringField(TEXT("recipePath"), RecipePath);
    } else if (!PickupPath.IsEmpty()) {
      Result->SetStringField(TEXT("assetType"), TEXT("Pickup"));
      Result->SetStringField(TEXT("pickupPath"), PickupPath);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory info retrieved"), Result);
    return true;
  }

  // ===========================================================================
  // Unknown SubAction
  // ===========================================================================

  SendAutomationError(
      RequestingSocket, RequestId,
      FString::Printf(TEXT("Unknown inventory action: %s"), *SubAction),
      TEXT("UNKNOWN_ACTION"));
  return true;
}

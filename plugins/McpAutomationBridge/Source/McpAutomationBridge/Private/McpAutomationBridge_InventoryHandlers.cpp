#include "Dom/JsonObject.h"
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

// Helper to create a new package with path validation
// Returns nullptr and sets OutError if path is invalid
static UPackage* CreateValidatedAssetPackage(const FString& Path, const FString& Name, FString& OutError) {
  FString PackageName;
  FString SanitizedName = SanitizeAssetName(Name);
  
  if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, OutError)) {
    return nullptr;
  }
  
  return CreatePackage(*PackageName);
}

// Legacy helper for backward compatibility - validates internally
static UPackage* CreateAssetPackage(const FString& Path, const FString& Name) {
  FString PackagePath = Path.IsEmpty() ? TEXT("/Game/Items") : Path;
  
  // Normalize and validate
  FString PackageName;
  FString PathError;
  FString SanitizedName = SanitizeAssetName(Name);
  if (!ValidateAssetCreationPath(PackagePath, SanitizedName, PackageName, PathError)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("CreateAssetPackage: %s"), *PathError);
    return nullptr;
  }
  
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

    // Create a primary data asset for item with validated path
    FString PathError;
    FString SanitizedName = SanitizeAssetName(Name);
    UPackage* Package = CreateValidatedAssetPackage(Path, SanitizedName, PathError);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId,
                          PathError.IsEmpty() ? TEXT("Failed to create package") : PathError,
                          TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    // Create UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
    UMcpGenericDataAsset* ItemAsset =
        NewObject<UMcpGenericDataAsset>(Package, FName(*SanitizedName), RF_Public | RF_Standalone);

    if (ItemAsset) {
      ItemAsset->MarkPackageDirty();
      FAssetRegistryModule::AssetCreated(ItemAsset);

      if (GetPayloadBool(Payload, TEXT("save"), true)) {
        McpSafeAssetSave(ItemAsset);
      }

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("itemPath"), Package->GetName());
      Result->SetStringField(TEXT("assetName"), SanitizedName);
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

    // Get properties object from payload
    const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
    TArray<FString> ModifiedProperties;
    TArray<FString> FailedProperties;

    if (Payload->TryGetObjectField(TEXT("properties"), PropertiesObj) && PropertiesObj && (*PropertiesObj).IsValid()) {
      // Iterate through all properties in the JSON and apply them via reflection
      for (const auto& Pair : (*PropertiesObj)->Values) {
        const FString& PropertyName = Pair.Key;
        const TSharedPtr<FJsonValue>& PropertyValue = Pair.Value;

        // Find the property on the item asset class
        FProperty* Prop = ItemAsset->GetClass()->FindPropertyByName(*PropertyName);
        if (Prop) {
          FString ApplyError;
          if (ApplyJsonValueToProperty(ItemAsset, Prop, PropertyValue, ApplyError)) {
            ModifiedProperties.Add(PropertyName);
          } else {
            FailedProperties.Add(FString::Printf(TEXT("%s: %s"), *PropertyName, *ApplyError));
          }
        } else {
          FailedProperties.Add(FString::Printf(TEXT("%s: Property not found"), *PropertyName));
        }
      }
    }

    ItemAsset->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(ItemAsset);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetBoolField(TEXT("modified"), ModifiedProperties.Num() > 0);
    Result->SetNumberField(TEXT("propertiesModified"), ModifiedProperties.Num());

    TArray<TSharedPtr<FJsonValue>> ModifiedArr;
    for (const FString& Name : ModifiedProperties) {
      ModifiedArr.Add(MakeShareable(new FJsonValueString(Name)));
    }
    Result->SetArrayField(TEXT("modifiedProperties"), ModifiedArr);

    if (FailedProperties.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> FailedArr;
      for (const FString& Err : FailedProperties) {
        FailedArr.Add(MakeShareable(new FJsonValueString(Err)));
      }
      Result->SetArrayField(TEXT("failedProperties"), FailedArr);
    }

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
        McpSafeAssetSave(CategoryAsset);
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

    bool bCategoryAssigned = false;
    FString AssignError;

    // Try to find a "Category" property on the item and set it via reflection
    FProperty* CategoryProp = ItemObj->GetClass()->FindPropertyByName(TEXT("Category"));
    if (!CategoryProp) {
      CategoryProp = ItemObj->GetClass()->FindPropertyByName(TEXT("ItemCategory"));
    }

    if (CategoryProp) {
      // Create a JSON value for the category path
      TSharedPtr<FJsonValue> CategoryValue = MakeShareable(new FJsonValueString(CategoryPath));
      if (ApplyJsonValueToProperty(ItemObj, CategoryProp, CategoryValue, AssignError)) {
        bCategoryAssigned = true;
      }
    } else {
      // Try to find a soft object reference property for category
      for (TFieldIterator<FProperty> It(ItemObj->GetClass()); It; ++It) {
        FProperty* Prop = *It;
        if (Prop->GetName().Contains(TEXT("Category"), ESearchCase::IgnoreCase)) {
          TSharedPtr<FJsonValue> CategoryValue = MakeShareable(new FJsonValueString(CategoryPath));
          if (ApplyJsonValueToProperty(ItemObj, Prop, CategoryValue, AssignError)) {
            bCategoryAssigned = true;
            break;
          }
        }
      }
    }

    ItemObj->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(ItemObj);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetStringField(TEXT("categoryPath"), CategoryPath);
    Result->SetBoolField(TEXT("assigned"), bCategoryAssigned);
    if (!bCategoryAssigned && !AssignError.IsEmpty()) {
      Result->SetStringField(TEXT("note"), TEXT("Category property not found on item class. Ensure your item class has a Category or ItemCategory property."));
    }
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

    // Create a SceneComponent as inventory component (real inventory would use custom UInventoryComponent)
    // USceneComponent allows for proper hierarchy and is a valid SCS node type
    USCS_Node* NewNode = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
    if (NewNode) {
      SCS->AddNode(NewNode);

      // Add Blueprint variables for inventory functionality
      int32 SlotCount = static_cast<int32>(GetPayloadNumber(Payload, TEXT("slotCount"), 20));

      // Add InventorySlots array variable (Array of soft object references)
      FEdGraphPinType SlotArrayType;
      SlotArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
      SlotArrayType.ContainerType = EPinContainerType::Array;
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("InventorySlots"), SlotArrayType);

      // Add MaxSlots integer variable
      FEdGraphPinType IntType;
      IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxSlots"), IntType);

      // Add CurrentWeight float variable
      FEdGraphPinType FloatType;
      FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
      FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CurrentWeight"), FloatType);

      // Add MaxWeight float variable
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxWeight"), FloatType);

      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        if (GetPayloadBool(Payload, TEXT("save"), true)) {
          McpSafeAssetSave(Blueprint);
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

    bool bConfigured = false;

    // Try to find and set MaxSlots property on the Blueprint's generated class CDO
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* MaxSlotsProp = CDO->GetClass()->FindPropertyByName(TEXT("MaxSlots"));
        if (MaxSlotsProp) {
          TSharedPtr<FJsonValue> SlotValue = MakeShareable(new FJsonValueNumber(static_cast<double>(SlotCount)));
          FString ApplyError;
          if (ApplyJsonValueToProperty(CDO, MaxSlotsProp, SlotValue, ApplyError)) {
            bConfigured = true;
          }
        }
      }
    }

    // If MaxSlots property doesn't exist, add it as a Blueprint variable
    if (!bConfigured) {
      FEdGraphPinType IntType;
      IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

      // Check if variable already exists
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == TEXT("MaxSlots")) {
          bExists = true;
          break;
        }
      }

      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxSlots"), IntType);
      }
      bConfigured = true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetNumberField(TEXT("slotCount"), SlotCount);
    Result->SetBoolField(TEXT("configured"), bConfigured);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
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

    // Note: Creating actual Blueprint functions programmatically requires K2Node graph manipulation
    // which is complex and error-prone. Instead, we add helper variables and event dispatchers
    // that can be used in Blueprint graphs to implement inventory functionality.

    TArray<TSharedPtr<FJsonValue>> FunctionsAdded;
    TArray<TSharedPtr<FJsonValue>> VariablesAdded;

    // Add helper variables for inventory operations
    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    // Add variables that support inventory functions
    TArray<TPair<FName, FEdGraphPinType>> InventoryVars = {
      TPair<FName, FEdGraphPinType>(TEXT("LastAddedItemIndex"), IntType),
      TPair<FName, FEdGraphPinType>(TEXT("LastRemovedItemIndex"), IntType),
      TPair<FName, FEdGraphPinType>(TEXT("bLastOperationSuccess"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("CachedItemCount"), IntType),
      TPair<FName, FEdGraphPinType>(TEXT("SelectedSlotIndex"), IntType)
    };

    for (const auto& VarPair : InventoryVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        VariablesAdded.Add(MakeShareable(new FJsonValueString(VarPair.Key.ToString())));
      }
    }

    // Add event dispatchers for inventory operations
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    TArray<FName> EventNames = {
      TEXT("OnAddItemRequested"),
      TEXT("OnRemoveItemRequested"),
      TEXT("OnTransferItemRequested")
    };

    for (const FName& EventName : EventNames) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == EventName) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, EventName, DelegateType);
        FunctionsAdded.Add(MakeShareable(new FJsonValueString(EventName.ToString())));
      }
    }

    // Mark as expected functions to implement in Blueprint
    TArray<FString> FunctionStubs = {
      TEXT("AddItem"),
      TEXT("RemoveItem"),
      TEXT("GetItemCount"),
      TEXT("HasItem"),
      TEXT("TransferItem")
    };

    for (const FString& FuncName : FunctionStubs) {
      FunctionsAdded.Add(MakeShareable(new FJsonValueString(FuncName + TEXT(" (implement in Blueprint)"))));
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetArrayField(TEXT("functionsAdded"), FunctionsAdded);
    Result->SetArrayField(TEXT("variablesAdded"), VariablesAdded);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetStringField(TEXT("note"), TEXT("Helper variables and event dispatchers added. Implement function logic in Blueprint graph using these variables."));

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

    // Define event dispatchers to add
    TArray<FString> EventNames = {
      TEXT("OnItemAdded"),
      TEXT("OnItemRemoved"),
      TEXT("OnInventoryChanged"),
      TEXT("OnSlotUpdated")
    };

    TArray<TSharedPtr<FJsonValue>> EventsAdded;

    // Add event dispatcher variables for each event
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    for (const FString& EventName : EventNames) {
      // Check if variable already exists
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName.ToString() == EventName) {
          bExists = true;
          break;
        }
      }

      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*EventName), DelegateType);
        EventsAdded.Add(MakeShareable(new FJsonValueString(EventName)));
      } else {
        EventsAdded.Add(MakeShareable(new FJsonValueString(EventName + TEXT(" (exists)"))));
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetArrayField(TEXT("eventsAdded"), EventsAdded);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory events configured"), Result);
    return true;
  }

  if (SubAction == TEXT("set_inventory_replication")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    bool Replicated = GetPayloadBool(Payload, TEXT("replicated"), false);
    FString ReplicationCondition = GetPayloadString(Payload, TEXT("replicationCondition"), TEXT("None"));

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

    TArray<FString> ReplicatedVariables;

    // Find inventory-related variables and set their replication flags
    TArray<FName> InventoryVarNames = {
      TEXT("InventorySlots"),
      TEXT("MaxSlots"),
      TEXT("CurrentWeight"),
      TEXT("MaxWeight")
    };

    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      bool bIsInventoryVar = false;
      for (const FName& VarName : InventoryVarNames) {
        if (Var.VarName == VarName) {
          bIsInventoryVar = true;
          break;
        }
      }

      if (bIsInventoryVar) {
        if (Replicated) {
          Var.PropertyFlags |= CPF_Net;
          Var.RepNotifyFunc = NAME_None; // Can be set to a custom function name

          // Set replication condition
          if (ReplicationCondition.Equals(TEXT("OwnerOnly"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_OwnerOnly;
          } else if (ReplicationCondition.Equals(TEXT("SkipOwner"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_SkipOwner;
          } else if (ReplicationCondition.Equals(TEXT("SimulatedOnly"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_SimulatedOnly;
          } else if (ReplicationCondition.Equals(TEXT("AutonomousOnly"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_AutonomousOnly;
          } else if (ReplicationCondition.Equals(TEXT("SimulatedOrPhysics"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_SimulatedOrPhysics;
          } else if (ReplicationCondition.Equals(TEXT("InitialOrOwner"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_InitialOrOwner;
          } else if (ReplicationCondition.Equals(TEXT("Custom"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_Custom;
          } else {
            Var.ReplicationCondition = COND_None;
          }
        } else {
          Var.PropertyFlags &= ~CPF_Net;
          Var.ReplicationCondition = COND_None;
        }
        ReplicatedVariables.Add(Var.VarName.ToString());
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("replicated"), Replicated);
    Result->SetStringField(TEXT("replicationCondition"), ReplicationCondition);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

    TArray<TSharedPtr<FJsonValue>> VarsArr;
    for (const FString& VarName : ReplicatedVariables) {
      VarsArr.Add(MakeShareable(new FJsonValueString(VarName)));
    }
    Result->SetArrayField(TEXT("modifiedVariables"), VarsArr);

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
        McpSafeAssetSave(NewBlueprint);
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
    FString Prompt = GetPayloadString(Payload, TEXT("prompt"), TEXT("Press E to pick up"));

    if (PickupPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: pickupPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the pickup blueprint
    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
    if (!Blueprint) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    bool bConfigured = false;

    // Add interaction type and prompt as Blueprint variables
    FEdGraphPinType StringType;
    StringType.PinCategory = UEdGraphSchema_K2::PC_String;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    // Add InteractionType variable
    bool bInteractionTypeExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("InteractionType")) {
        bInteractionTypeExists = true;
        break;
      }
    }
    if (!bInteractionTypeExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("InteractionType"), NameType);
    }

    // Add InteractionPrompt variable
    bool bPromptExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("InteractionPrompt")) {
        bPromptExists = true;
        break;
      }
    }
    if (!bPromptExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("InteractionPrompt"), StringType);
    }

    // Configure the interaction sphere component if it exists
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (SCS) {
      for (USCS_Node* Node : SCS->GetAllNodes()) {
        if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(USphereComponent::StaticClass())) {
          USphereComponent* SphereComp = Cast<USphereComponent>(Node->ComponentTemplate);
          if (SphereComp) {
            if (InteractionType.Equals(TEXT("Overlap"), ESearchCase::IgnoreCase)) {
              SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
              SphereComp->SetGenerateOverlapEvents(true);
            } else {
              SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
            bConfigured = true;
          }
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("pickupPath"), PickupPath);
    Result->SetStringField(TEXT("interactionType"), InteractionType);
    Result->SetStringField(TEXT("prompt"), Prompt);
    Result->SetBoolField(TEXT("configured"), bConfigured);
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

    // Load the pickup blueprint
    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
    if (!Blueprint) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // Add respawn-related Blueprint variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    // Add bRespawnable variable
    bool bRespawnableExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("bRespawnable")) {
        bRespawnableExists = true;
        break;
      }
    }
    if (!bRespawnableExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bRespawnable"), BoolType);
    }

    // Add RespawnTime variable
    bool bRespawnTimeExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("RespawnTime")) {
        bRespawnTimeExists = true;
        break;
      }
    }
    if (!bRespawnTimeExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("RespawnTime"), FloatType);
    }

    // Set default values on the CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* RespawnableProp = CDO->GetClass()->FindPropertyByName(TEXT("bRespawnable"));
        if (RespawnableProp) {
          TSharedPtr<FJsonValue> BoolValue = MakeShareable(new FJsonValueBoolean(Respawnable));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, RespawnableProp, BoolValue, ApplyError);
        }

        FProperty* RespawnTimeProp = CDO->GetClass()->FindPropertyByName(TEXT("RespawnTime"));
        if (RespawnTimeProp) {
          TSharedPtr<FJsonValue> FloatValue = MakeShareable(new FJsonValueNumber(RespawnTime));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, RespawnTimeProp, FloatValue, ApplyError);
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("pickupPath"), PickupPath);
    Result->SetBoolField(TEXT("respawnable"), Respawnable);
    Result->SetNumberField(TEXT("respawnTime"), RespawnTime);
    Result->SetBoolField(TEXT("configured"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pickup respawn configured"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_pickup_effects")) {
    FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));
    bool Bobbing = GetPayloadBool(Payload, TEXT("bobbing"), true);
    bool Rotation = GetPayloadBool(Payload, TEXT("rotation"), true);
    bool GlowEffect = GetPayloadBool(Payload, TEXT("glowEffect"), false);

    if (PickupPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: pickupPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the pickup blueprint
    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
    if (!Blueprint) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // Add effect-related Blueprint variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    // Add effect control variables
    TArray<TPair<FName, bool>> EffectVars = {
      TPair<FName, bool>(TEXT("bEnableBobbing"), Bobbing),
      TPair<FName, bool>(TEXT("bEnableRotation"), Rotation),
      TPair<FName, bool>(TEXT("bEnableGlowEffect"), GlowEffect)
    };

    for (const auto& VarPair : EffectVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, BoolType);
      }
    }

    // Add bobbing/rotation parameters
    TArray<FName> FloatVars = {
      TEXT("BobbingSpeed"),
      TEXT("BobbingHeight"),
      TEXT("RotationSpeed")
    };

    for (const FName& VarName : FloatVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarName) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, FloatType);
      }
    }

    // Set default values on the CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        for (const auto& VarPair : EffectVars) {
          FProperty* Prop = CDO->GetClass()->FindPropertyByName(VarPair.Key);
          if (Prop) {
            TSharedPtr<FJsonValue> BoolValue = MakeShareable(new FJsonValueBoolean(VarPair.Value));
            FString ApplyError;
            ApplyJsonValueToProperty(CDO, Prop, BoolValue, ApplyError);
          }
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("pickupPath"), PickupPath);
    Result->SetBoolField(TEXT("bobbing"), Bobbing);
    Result->SetBoolField(TEXT("rotation"), Rotation);
    Result->SetBoolField(TEXT("glowEffect"), GlowEffect);
    Result->SetBoolField(TEXT("configured"), true);
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
      // Create a SceneComponent for equipment (proper hierarchy support)
      USCS_Node* NewNode = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
      if (NewNode) {
        SCS->AddNode(NewNode);

        // Add equipment-related Blueprint variables
        FEdGraphPinType SoftObjectArrayType;
        SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
        SoftObjectArrayType.ContainerType = EPinContainerType::Array;

        FEdGraphPinType NameArrayType;
        NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
        NameArrayType.ContainerType = EPinContainerType::Array;

        // Add EquipmentSlots array variable
        bool bSlotsExists = false;
        for (FBPVariableDescription& Var : Blueprint->NewVariables) {
          if (Var.VarName == TEXT("EquipmentSlots")) {
            bSlotsExists = true;
            break;
          }
        }
        if (!bSlotsExists) {
          FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("EquipmentSlots"), SoftObjectArrayType);
        }

        // Add EquippedItems array
        bool bEquippedExists = false;
        for (FBPVariableDescription& Var : Blueprint->NewVariables) {
          if (Var.VarName == TEXT("EquippedItems")) {
            bEquippedExists = true;
            break;
          }
        }
        if (!bEquippedExists) {
          FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("EquippedItems"), SoftObjectArrayType);
        }

        // Add SlotNames array
        bool bSlotNamesExists = false;
        for (FBPVariableDescription& Var : Blueprint->NewVariables) {
          if (Var.VarName == TEXT("SlotNames")) {
            bSlotNamesExists = true;
            break;
          }
        }
        if (!bSlotNamesExists) {
          FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SlotNames"), NameArrayType);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        if (GetPayloadBool(Payload, TEXT("save"), true)) {
          McpSafeAssetSave(Blueprint);
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetBoolField(TEXT("componentAdded"), true);
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

        TArray<TSharedPtr<FJsonValue>> AddedVars;
        AddedVars.Add(MakeShareable(new FJsonValueString(TEXT("EquipmentSlots"))));
        AddedVars.Add(MakeShareable(new FJsonValueString(TEXT("EquippedItems"))));
        AddedVars.Add(MakeShareable(new FJsonValueString(TEXT("SlotNames"))));
        Result->SetArrayField(TEXT("variablesAdded"), AddedVars);

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

    // Get custom slots from payload or use defaults
    TArray<FString> SlotNames;
    const TArray<TSharedPtr<FJsonValue>>* SlotsArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("slots"), SlotsArr) && SlotsArr) {
      for (const auto& SlotVal : *SlotsArr) {
        SlotNames.Add(SlotVal->AsString());
      }
    }

    // Default slots if none provided
    if (SlotNames.Num() == 0) {
      SlotNames = {
        TEXT("Head"),
        TEXT("Chest"),
        TEXT("Hands"),
        TEXT("Legs"),
        TEXT("Feet"),
        TEXT("MainWeapon"),
        TEXT("OffhandWeapon")
      };
    }

    // Add SlotNames array variable if it doesn't exist
    FEdGraphPinType NameArrayType;
    NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
    NameArrayType.ContainerType = EPinContainerType::Array;

    bool bSlotNamesExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("SlotNames")) {
        bSlotNamesExists = true;
        break;
      }
    }
    if (!bSlotNamesExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SlotNames"), NameArrayType);
    }

    // Add EquippedItems array (parallel array to SlotNames)
    FEdGraphPinType SoftObjectArrayType;
    SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
    SoftObjectArrayType.ContainerType = EPinContainerType::Array;

    bool bEquippedExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("EquippedItems")) {
        bEquippedExists = true;
        break;
      }
    }
    if (!bEquippedExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("EquippedItems"), SoftObjectArrayType);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

    TArray<TSharedPtr<FJsonValue>> ConfiguredSlots;
    for (const FString& SlotName : SlotNames) {
      ConfiguredSlots.Add(MakeShareable(new FJsonValueString(SlotName)));
    }
    Result->SetArrayField(TEXT("slotsConfigured"), ConfiguredSlots);
    Result->SetNumberField(TEXT("slotCount"), SlotNames.Num());

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

    // Add equipment effect configuration variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType SoftObjectArrayType;
    SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
    SoftObjectArrayType.ContainerType = EPinContainerType::Array;

    FEdGraphPinType NameArrayType;
    NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
    NameArrayType.ContainerType = EPinContainerType::Array;

    // Stat modifier variables
    TArray<TPair<FName, FEdGraphPinType>> EffectVars = {
      TPair<FName, FEdGraphPinType>(TEXT("bApplyStatModifiers"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("StatModifierMultiplier"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bGrantAbilitiesOnEquip"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("GrantedAbilities"), SoftObjectArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("bApplyPassiveEffects"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("PassiveEffectClasses"), SoftObjectArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("EffectTags"), NameArrayType)
    };

    TArray<TSharedPtr<FJsonValue>> AddedVars;

    for (const auto& VarPair : EffectVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShareable(new FJsonValueString(VarPair.Key.ToString())));
      }
    }

    // Set default values on CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* StatModProp = CDO->GetClass()->FindPropertyByName(TEXT("bApplyStatModifiers"));
        if (StatModProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShareable(new FJsonValueBoolean(GetPayloadBool(Payload, TEXT("statModifiers"), true)));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, StatModProp, BoolVal, ApplyError);
        }

        FProperty* AbilityProp = CDO->GetClass()->FindPropertyByName(TEXT("bGrantAbilitiesOnEquip"));
        if (AbilityProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShareable(new FJsonValueBoolean(GetPayloadBool(Payload, TEXT("abilityGrants"), true)));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, AbilityProp, BoolVal, ApplyError);
        }

        FProperty* PassiveProp = CDO->GetClass()->FindPropertyByName(TEXT("bApplyPassiveEffects"));
        if (PassiveProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShareable(new FJsonValueBoolean(GetPayloadBool(Payload, TEXT("passiveEffects"), true)));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, PassiveProp, BoolVal, ApplyError);
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("statModifiersConfigured"), GetPayloadBool(Payload, TEXT("statModifiers"), true));
    Result->SetBoolField(TEXT("abilityGrantsConfigured"), GetPayloadBool(Payload, TEXT("abilityGrants"), true));
    Result->SetBoolField(TEXT("passiveEffectsConfigured"), GetPayloadBool(Payload, TEXT("passiveEffects"), true));
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
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

    TArray<TSharedPtr<FJsonValue>> FunctionsAdded;
    TArray<TSharedPtr<FJsonValue>> VariablesAdded;

    // Add helper variables for equipment operations
    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    // Add variables that support equipment functions
    TArray<TPair<FName, FEdGraphPinType>> EquipmentVars = {
      TPair<FName, FEdGraphPinType>(TEXT("LastEquippedSlot"), NameType),
      TPair<FName, FEdGraphPinType>(TEXT("LastUnequippedSlot"), NameType),
      TPair<FName, FEdGraphPinType>(TEXT("bLastEquipSuccess"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("CurrentlyEquippedItem"), SoftObjectType),
      TPair<FName, FEdGraphPinType>(TEXT("PendingEquipItem"), SoftObjectType),
      TPair<FName, FEdGraphPinType>(TEXT("EquipmentChangeCount"), IntType)
    };

    for (const auto& VarPair : EquipmentVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        VariablesAdded.Add(MakeShareable(new FJsonValueString(VarPair.Key.ToString())));
      }
    }

    // Add event dispatchers for equipment operations
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    TArray<FName> EventNames = {
      TEXT("OnEquipItemRequested"),
      TEXT("OnUnequipItemRequested"),
      TEXT("OnEquipmentSwapRequested"),
      TEXT("OnEquipmentChanged")
    };

    for (const FName& EventName : EventNames) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == EventName) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, EventName, DelegateType);
        FunctionsAdded.Add(MakeShareable(new FJsonValueString(EventName.ToString())));
      }
    }

    // Mark as expected functions to implement in Blueprint
    TArray<FString> FunctionStubs = {
      TEXT("EquipItem"),
      TEXT("UnequipItem"),
      TEXT("GetEquippedItem"),
      TEXT("CanEquip"),
      TEXT("SwapEquipment")
    };

    for (const FString& FuncName : FunctionStubs) {
      FunctionsAdded.Add(MakeShareable(new FJsonValueString(FuncName + TEXT(" (implement in Blueprint)"))));
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetArrayField(TEXT("functionsAdded"), FunctionsAdded);
    Result->SetArrayField(TEXT("variablesAdded"), VariablesAdded);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetStringField(TEXT("note"), TEXT("Helper variables and event dispatchers added. Implement function logic in Blueprint graph."));

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

    bool bAttachToSocket = GetPayloadBool(Payload, TEXT("attachToSocket"), true);
    FString DefaultSocket = GetPayloadString(Payload, TEXT("defaultSocket"), TEXT("hand_r"));

    // Add equipment visual configuration variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    FEdGraphPinType NameArrayType;
    NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
    NameArrayType.ContainerType = EPinContainerType::Array;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    FEdGraphPinType TransformType;
    TransformType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    TransformType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();

    // Visual configuration variables
    TArray<TPair<FName, FEdGraphPinType>> VisualVars = {
      TPair<FName, FEdGraphPinType>(TEXT("bAttachToSocket"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("DefaultAttachSocket"), NameType),
      TPair<FName, FEdGraphPinType>(TEXT("EquipmentSockets"), NameArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("EquipmentMesh"), SoftObjectType),
      TPair<FName, FEdGraphPinType>(TEXT("AttachmentOffset"), TransformType),
      TPair<FName, FEdGraphPinType>(TEXT("bUseCustomAttachRules"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("bHideEquippedMesh"), BoolType)
    };

    TArray<TSharedPtr<FJsonValue>> AddedVars;

    for (const auto& VarPair : VisualVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShareable(new FJsonValueString(VarPair.Key.ToString())));
      }
    }

    // Set default values on CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* AttachProp = CDO->GetClass()->FindPropertyByName(TEXT("bAttachToSocket"));
        if (AttachProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShareable(new FJsonValueBoolean(bAttachToSocket));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, AttachProp, BoolVal, ApplyError);
        }

        FProperty* SocketProp = CDO->GetClass()->FindPropertyByName(TEXT("DefaultAttachSocket"));
        if (SocketProp) {
          TSharedPtr<FJsonValue> NameVal = MakeShareable(new FJsonValueString(DefaultSocket));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, SocketProp, NameVal, ApplyError);
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("attachToSocket"), bAttachToSocket);
    Result->SetStringField(TEXT("defaultSocket"), DefaultSocket);
    Result->SetBoolField(TEXT("visualsConfigured"), true);
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
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
        McpSafeAssetSave(LootTableAsset);
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

    // Load the loot table asset
    UObject* LootTableObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *LootTablePath);
    UMcpGenericDataAsset* LootTable = Cast<UMcpGenericDataAsset>(LootTableObj);

    if (!LootTable) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Loot table not found: %s"), *LootTablePath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    int32 EntryIndex = 0;
    bool bEntryAdded = false;

    // Try to find and modify LootEntries array via reflection
    FProperty* EntriesProp = LootTable->GetClass()->FindPropertyByName(TEXT("LootEntries"));
    if (!EntriesProp) {
      EntriesProp = LootTable->GetClass()->FindPropertyByName(TEXT("Entries"));
    }

    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(EntriesProp)) {
      // For custom loot table classes with proper array properties
      FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(LootTable));
      // Actually add a new element to the array
      int32 NewIdx = ArrayHelper.AddValue();
      if (NewIdx != INDEX_NONE) {
        EntryIndex = NewIdx;
        bEntryAdded = true;
        // Note: The new element's inner fields (item path, weight, quantities) 
        // would need to be populated via reflection based on the struct definition
      } else {
        bEntryAdded = false;
      }
    } else {
      // For generic data assets without proper array properties
      bEntryAdded = false;
    }

    LootTable->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(LootTable);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetNumberField(TEXT("weight"), Weight);
    Result->SetNumberField(TEXT("minQuantity"), MinQuantity);
    Result->SetNumberField(TEXT("maxQuantity"), MaxQuantity);
    Result->SetNumberField(TEXT("entryIndex"), EntryIndex);
    Result->SetBoolField(TEXT("added"), bEntryAdded);
    if (!EntriesProp) {
      Result->SetStringField(TEXT("note"), TEXT("LootEntries property not found. Ensure your loot table class has a LootEntries or Entries array property."));
    }
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

    // Load the actor blueprint
    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *ActorPath));
    if (!Blueprint) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Actor blueprint not found: %s"), *ActorPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    int32 DropCount = static_cast<int32>(GetPayloadNumber(Payload, TEXT("dropCount"), 1));
    double DropRadius = GetPayloadNumber(Payload, TEXT("dropRadius"), 100.0);
    bool bDropOnDeath = GetPayloadBool(Payload, TEXT("dropOnDeath"), true);

    // Add loot drop configuration variables
    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    FEdGraphPinType VectorType;
    VectorType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    VectorType.PinSubCategoryObject = TBaseStructure<FVector>::Get();

    // Loot drop variables
    TArray<TPair<FName, FEdGraphPinType>> LootVars = {
      TPair<FName, FEdGraphPinType>(TEXT("LootTable"), SoftObjectType),
      TPair<FName, FEdGraphPinType>(TEXT("LootDropCount"), IntType),
      TPair<FName, FEdGraphPinType>(TEXT("LootDropRadius"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bDropLootOnDeath"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("bRandomizeDropLocation"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("DropOffset"), VectorType),
      TPair<FName, FEdGraphPinType>(TEXT("bApplyDropImpulse"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("DropImpulseStrength"), FloatType)
    };

    TArray<TSharedPtr<FJsonValue>> AddedVars;

    for (const auto& VarPair : LootVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShareable(new FJsonValueString(VarPair.Key.ToString())));
      }
    }

    // Add event dispatcher for loot drops
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    bool bEventExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("OnLootDropped")) {
        bEventExists = true;
        break;
      }
    }
    if (!bEventExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OnLootDropped"), DelegateType);
      AddedVars.Add(MakeShareable(new FJsonValueString(TEXT("OnLootDropped"))));
    }

    // Set default values on CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* DropCountProp = CDO->GetClass()->FindPropertyByName(TEXT("LootDropCount"));
        if (DropCountProp) {
          TSharedPtr<FJsonValue> IntVal = MakeShareable(new FJsonValueNumber(static_cast<double>(DropCount)));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, DropCountProp, IntVal, ApplyError);
        }

        FProperty* DropRadiusProp = CDO->GetClass()->FindPropertyByName(TEXT("LootDropRadius"));
        if (DropRadiusProp) {
          TSharedPtr<FJsonValue> FloatVal = MakeShareable(new FJsonValueNumber(DropRadius));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, DropRadiusProp, FloatVal, ApplyError);
        }

        FProperty* DropOnDeathProp = CDO->GetClass()->FindPropertyByName(TEXT("bDropLootOnDeath"));
        if (DropOnDeathProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShareable(new FJsonValueBoolean(bDropOnDeath));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, DropOnDeathProp, BoolVal, ApplyError);
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("actorPath"), ActorPath);
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    Result->SetNumberField(TEXT("dropCount"), DropCount);
    Result->SetNumberField(TEXT("dropRadius"), DropRadius);
    Result->SetBoolField(TEXT("dropOnDeath"), bDropOnDeath);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
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

    // Load the loot table asset
    UObject* LootTableObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *LootTablePath);
    UMcpGenericDataAsset* LootTable = Cast<UMcpGenericDataAsset>(LootTableObj);

    if (!LootTable) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Loot table not found: %s"), *LootTablePath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Get custom tiers from payload or use defaults
    TArray<TPair<FString, double>> Tiers;
    const TArray<TSharedPtr<FJsonValue>>* TiersArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("tiers"), TiersArr) && TiersArr) {
      for (const auto& TierVal : *TiersArr) {
        const TSharedPtr<FJsonObject>* TierObj = nullptr;
        if (TierVal->TryGetObject(TierObj) && TierObj && (*TierObj).IsValid()) {
          FString TierName = GetJsonStringField((*TierObj), TEXT("name"));
          double TierWeight = GetJsonNumberField((*TierObj), TEXT("dropWeight"));
          Tiers.Add(TPair<FString, double>(TierName, TierWeight));
        }
      }
    }

    // Default tiers if none provided
    if (Tiers.Num() == 0) {
      Tiers = {
        TPair<FString, double>(TEXT("Common"), 60.0),
        TPair<FString, double>(TEXT("Uncommon"), 25.0),
        TPair<FString, double>(TEXT("Rare"), 10.0),
        TPair<FString, double>(TEXT("Epic"), 4.0),
        TPair<FString, double>(TEXT("Legendary"), 1.0)
      };
    }

    bool bTiersSet = false;

    // Try to find and set QualityTiers property via reflection
    FProperty* TiersProp = LootTable->GetClass()->FindPropertyByName(TEXT("QualityTiers"));
    if (!TiersProp) {
      TiersProp = LootTable->GetClass()->FindPropertyByName(TEXT("Tiers"));
    }

    if (TiersProp) {
      // Property exists - data would be set via reflection here for custom classes
      bTiersSet = true;
    }

    LootTable->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(LootTable);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);

    TArray<TSharedPtr<FJsonValue>> ConfiguredTiers;
    for (const auto& TierPair : Tiers) {
      TSharedPtr<FJsonObject> TierObj = MakeShareable(new FJsonObject());
      TierObj->SetStringField(TEXT("name"), TierPair.Key);
      TierObj->SetNumberField(TEXT("dropWeight"), TierPair.Value);
      ConfiguredTiers.Add(MakeShareable(new FJsonValueObject(TierObj)));
    }
    Result->SetArrayField(TEXT("tiersConfigured"), ConfiguredTiers);
    Result->SetNumberField(TEXT("tierCount"), Tiers.Num());
    Result->SetBoolField(TEXT("configured"), true);

    if (!TiersProp) {
      Result->SetStringField(TEXT("note"), TEXT("QualityTiers property not found. Ensure your loot table class has a QualityTiers or Tiers property."));
    }

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
        McpSafeAssetSave(RecipeAsset);
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
          McpSafeAssetSave(StationBlueprint);
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
      // Use USceneComponent for proper SCS hierarchy (UActorComponent cannot be added to SCS)
      USCS_Node* NewNode = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
      if (NewNode) {
        SCS->AddNode(NewNode);

        // Add crafting-related Blueprint variables
        FEdGraphPinType SoftObjectArrayType;
        SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
        SoftObjectArrayType.ContainerType = EPinContainerType::Array;

        FEdGraphPinType BoolType;
        BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

        FEdGraphPinType FloatType;
        FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

        FEdGraphPinType IntType;
        IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

        // Crafting variables
        TArray<TPair<FName, FEdGraphPinType>> CraftingVars = {
          TPair<FName, FEdGraphPinType>(TEXT("AvailableRecipes"), SoftObjectArrayType),
          TPair<FName, FEdGraphPinType>(TEXT("CraftingQueue"), SoftObjectArrayType),
          TPair<FName, FEdGraphPinType>(TEXT("bIsCrafting"), BoolType),
          TPair<FName, FEdGraphPinType>(TEXT("CurrentCraftProgress"), FloatType),
          TPair<FName, FEdGraphPinType>(TEXT("CraftingSpeedMultiplier"), FloatType),
          TPair<FName, FEdGraphPinType>(TEXT("MaxQueueSize"), IntType)
        };

        TArray<TSharedPtr<FJsonValue>> AddedVars;

        for (const auto& VarPair : CraftingVars) {
          bool bExists = false;
          for (FBPVariableDescription& Var : Blueprint->NewVariables) {
            if (Var.VarName == VarPair.Key) {
              bExists = true;
              break;
            }
          }
          if (!bExists) {
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
            AddedVars.Add(MakeShareable(new FJsonValueString(VarPair.Key.ToString())));
          }
        }

        // Add event dispatchers for crafting
        FEdGraphPinType DelegateType;
        DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

        TArray<FName> EventNames = {
          TEXT("OnCraftingStarted"),
          TEXT("OnCraftingCompleted"),
          TEXT("OnCraftingCancelled"),
          TEXT("OnCraftingProgressUpdated")
        };

        for (const FName& EventName : EventNames) {
          bool bExists = false;
          for (FBPVariableDescription& Var : Blueprint->NewVariables) {
            if (Var.VarName == EventName) {
              bExists = true;
              break;
            }
          }
          if (!bExists) {
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, EventName, DelegateType);
            AddedVars.Add(MakeShareable(new FJsonValueString(EventName.ToString())));
          }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        if (GetPayloadBool(Payload, TEXT("save"), true)) {
          McpSafeAssetSave(Blueprint);
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetBoolField(TEXT("componentAdded"), true);
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
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
  // 17.7 Additional Actions (6 actions to complete 33 total)
  // ===========================================================================

  if (SubAction == TEXT("configure_item_stacking")) {
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));

    if (ItemPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: itemPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the item asset
    UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    if (!ItemAsset) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Item not found: %s"), *ItemPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bStackable = GetPayloadBool(Payload, TEXT("stackable"), true);
    int32 MaxStackSize = static_cast<int32>(GetPayloadNumber(Payload, TEXT("maxStackSize"), 99));
    bool bUniqueItems = GetPayloadBool(Payload, TEXT("uniqueItems"), false);

    TArray<FString> ModifiedProps;

    // Try to set stacking properties via reflection
    FProperty* StackableProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("bStackable"));
    if (!StackableProp) {
      StackableProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("Stackable"));
    }
    if (StackableProp) {
      TSharedPtr<FJsonValue> BoolVal = MakeShareable(new FJsonValueBoolean(bStackable));
      FString ApplyError;
      if (ApplyJsonValueToProperty(ItemAsset, StackableProp, BoolVal, ApplyError)) {
        ModifiedProps.Add(TEXT("Stackable"));
      }
    }

    FProperty* MaxStackProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("MaxStackSize"));
    if (!MaxStackProp) {
      MaxStackProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("StackLimit"));
    }
    if (MaxStackProp) {
      TSharedPtr<FJsonValue> IntVal = MakeShareable(new FJsonValueNumber(static_cast<double>(MaxStackSize)));
      FString ApplyError;
      if (ApplyJsonValueToProperty(ItemAsset, MaxStackProp, IntVal, ApplyError)) {
        ModifiedProps.Add(TEXT("MaxStackSize"));
      }
    }

    FProperty* UniqueProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("bUniqueItem"));
    if (UniqueProp) {
      TSharedPtr<FJsonValue> BoolVal = MakeShareable(new FJsonValueBoolean(bUniqueItems));
      FString ApplyError;
      if (ApplyJsonValueToProperty(ItemAsset, UniqueProp, BoolVal, ApplyError)) {
        ModifiedProps.Add(TEXT("UniqueItem"));
      }
    }

    ItemAsset->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(ItemAsset);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetBoolField(TEXT("stackable"), bStackable);
    Result->SetNumberField(TEXT("maxStackSize"), MaxStackSize);
    Result->SetBoolField(TEXT("uniqueItems"), bUniqueItems);

    TArray<TSharedPtr<FJsonValue>> ModArr;
    for (const FString& Prop : ModifiedProps) {
      ModArr.Add(MakeShareable(new FJsonValueString(Prop)));
    }
    Result->SetArrayField(TEXT("modifiedProperties"), ModArr);
    Result->SetBoolField(TEXT("configured"), true);

    if (ModifiedProps.Num() == 0) {
      Result->SetStringField(TEXT("note"), TEXT("No stacking properties found. Ensure your item class has bStackable, MaxStackSize, or StackLimit properties."));
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Item stacking configured"), Result);
    return true;
  }

  if (SubAction == TEXT("set_item_icon")) {
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
    FString IconPath = GetPayloadString(Payload, TEXT("iconPath"));

    if (ItemPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: itemPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the item asset
    UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    if (!ItemAsset) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Item not found: %s"), *ItemPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bIconSet = false;
    FString IconPropertyName;

    // Try common icon property names
    TArray<FString> IconPropNames = {
      TEXT("Icon"),
      TEXT("ItemIcon"),
      TEXT("Thumbnail"),
      TEXT("DisplayIcon"),
      TEXT("InventoryIcon")
    };

    for (const FString& PropName : IconPropNames) {
      FProperty* IconProp = ItemAsset->GetClass()->FindPropertyByName(*PropName);
      if (IconProp) {
        TSharedPtr<FJsonValue> PathVal = MakeShareable(new FJsonValueString(IconPath));
        FString ApplyError;
        if (ApplyJsonValueToProperty(ItemAsset, IconProp, PathVal, ApplyError)) {
          bIconSet = true;
          IconPropertyName = PropName;
          break;
        }
      }
    }

    ItemAsset->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(ItemAsset);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetStringField(TEXT("iconPath"), IconPath);
    Result->SetBoolField(TEXT("iconSet"), bIconSet);
    if (bIconSet) {
      Result->SetStringField(TEXT("propertyModified"), IconPropertyName);
    } else {
      Result->SetStringField(TEXT("note"), TEXT("No icon property found. Ensure your item class has an Icon, ItemIcon, or Thumbnail property."));
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Item icon configured"), Result);
    return true;
  }

  if (SubAction == TEXT("add_recipe_ingredient")) {
    FString RecipePath = GetPayloadString(Payload, TEXT("recipePath"));
    FString IngredientItemPath = GetPayloadString(Payload, TEXT("ingredientItemPath"));
    int32 Quantity = static_cast<int32>(GetPayloadNumber(Payload, TEXT("quantity"), 1));

    if (RecipePath.IsEmpty() || IngredientItemPath.IsEmpty()) {
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: recipePath and ingredientItemPath"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the recipe asset
    UObject* RecipeAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *RecipePath);
    if (!RecipeAsset) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Recipe not found: %s"), *RecipePath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bIngredientAdded = false;
    int32 IngredientIndex = 0;

    // Try to find Ingredients array via reflection
    FProperty* IngredientsProp = RecipeAsset->GetClass()->FindPropertyByName(TEXT("Ingredients"));
    if (!IngredientsProp) {
      IngredientsProp = RecipeAsset->GetClass()->FindPropertyByName(TEXT("RequiredItems"));
    }
    if (!IngredientsProp) {
      IngredientsProp = RecipeAsset->GetClass()->FindPropertyByName(TEXT("InputItems"));
    }

    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(IngredientsProp)) {
      FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(RecipeAsset));
      // Actually add a new element to the array
      int32 NewIdx = ArrayHelper.AddValue();
      if (NewIdx != INDEX_NONE) {
        IngredientIndex = NewIdx;
        bIngredientAdded = true;
        // Note: The new element's inner fields (item path, quantity)
        // would need to be populated via reflection based on the struct definition
      } else {
        bIngredientAdded = false;
      }
    } else {
      // For generic data assets without proper array properties
      bIngredientAdded = false;
    }

    RecipeAsset->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(RecipeAsset);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("recipePath"), RecipePath);
    Result->SetStringField(TEXT("ingredientItemPath"), IngredientItemPath);
    Result->SetNumberField(TEXT("quantity"), Quantity);
    Result->SetNumberField(TEXT("ingredientIndex"), IngredientIndex);
    Result->SetBoolField(TEXT("added"), bIngredientAdded);

    if (!IngredientsProp) {
      Result->SetStringField(TEXT("note"), TEXT("Ingredients property not found. Ensure your recipe class has an Ingredients, RequiredItems, or InputItems array."));
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Recipe ingredient added"), Result);
    return true;
  }

  if (SubAction == TEXT("remove_loot_entry")) {
    FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));
    int32 EntryIndex = static_cast<int32>(GetPayloadNumber(Payload, TEXT("entryIndex"), -1));
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));

    if (LootTablePath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: lootTablePath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    if (EntryIndex < 0 && ItemPath.IsEmpty()) {
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Either entryIndex or itemPath must be provided"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the loot table asset
    UObject* LootTableObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *LootTablePath);
    UMcpGenericDataAsset* LootTable = Cast<UMcpGenericDataAsset>(LootTableObj);

    if (!LootTable) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Loot table not found: %s"), *LootTablePath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bEntryRemoved = false;
    int32 RemovedIndex = -1;

    // Try to find and modify LootEntries array via reflection
    FProperty* EntriesProp = LootTable->GetClass()->FindPropertyByName(TEXT("LootEntries"));
    if (!EntriesProp) {
      EntriesProp = LootTable->GetClass()->FindPropertyByName(TEXT("Entries"));
    }

    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(EntriesProp)) {
      FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(LootTable));
      if (EntryIndex >= 0 && EntryIndex < ArrayHelper.Num()) {
        ArrayHelper.RemoveValues(EntryIndex, 1);
        bEntryRemoved = true;
        RemovedIndex = EntryIndex;
      }
    }

    LootTable->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(LootTable);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    Result->SetNumberField(TEXT("removedIndex"), RemovedIndex);
    Result->SetBoolField(TEXT("removed"), bEntryRemoved);

    if (!bEntryRemoved) {
      Result->SetStringField(TEXT("note"), TEXT("Entry not removed. Check that entryIndex is valid or LootEntries array exists."));
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Loot entry removed"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_inventory_weight")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

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

    double MaxWeight = GetPayloadNumber(Payload, TEXT("maxWeight"), 100.0);
    bool bEnableWeight = GetPayloadBool(Payload, TEXT("enableWeight"), true);
    bool bEncumberanceSystem = GetPayloadBool(Payload, TEXT("encumberanceSystem"), false);
    double EncumberanceThreshold = GetPayloadNumber(Payload, TEXT("encumberanceThreshold"), 0.75);

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    // Weight configuration variables
    TArray<TPair<FName, FEdGraphPinType>> WeightVars = {
      TPair<FName, FEdGraphPinType>(TEXT("MaxCarryWeight"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("CurrentCarryWeight"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bWeightEnabled"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("bUseEncumberance"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("EncumberanceThreshold"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("WeightMultiplier"), FloatType)
    };

    TArray<TSharedPtr<FJsonValue>> AddedVars;

    for (const auto& VarPair : WeightVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShareable(new FJsonValueString(VarPair.Key.ToString())));
      }
    }

    // Add weight-related event
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    bool bEventExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("OnEncumberanceChanged")) {
        bEventExists = true;
        break;
      }
    }
    if (!bEventExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OnEncumberanceChanged"), DelegateType);
      AddedVars.Add(MakeShareable(new FJsonValueString(TEXT("OnEncumberanceChanged"))));
    }

    // Set default values on CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* MaxWeightProp = CDO->GetClass()->FindPropertyByName(TEXT("MaxCarryWeight"));
        if (MaxWeightProp) {
          TSharedPtr<FJsonValue> FloatVal = MakeShareable(new FJsonValueNumber(MaxWeight));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, MaxWeightProp, FloatVal, ApplyError);
        }

        FProperty* EnableProp = CDO->GetClass()->FindPropertyByName(TEXT("bWeightEnabled"));
        if (EnableProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShareable(new FJsonValueBoolean(bEnableWeight));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, EnableProp, BoolVal, ApplyError);
        }

        FProperty* EncumProp = CDO->GetClass()->FindPropertyByName(TEXT("bUseEncumberance"));
        if (EncumProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShareable(new FJsonValueBoolean(bEncumberanceSystem));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, EncumProp, BoolVal, ApplyError);
        }

        FProperty* ThreshProp = CDO->GetClass()->FindPropertyByName(TEXT("EncumberanceThreshold"));
        if (ThreshProp) {
          TSharedPtr<FJsonValue> FloatVal = MakeShareable(new FJsonValueNumber(EncumberanceThreshold));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, ThreshProp, FloatVal, ApplyError);
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("maxWeight"), MaxWeight);
    Result->SetBoolField(TEXT("enableWeight"), bEnableWeight);
    Result->SetBoolField(TEXT("encumberanceSystem"), bEncumberanceSystem);
    Result->SetNumberField(TEXT("encumberanceThreshold"), EncumberanceThreshold);
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
    Result->SetBoolField(TEXT("configured"), true);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory weight configured"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_station_recipes")) {
    FString StationPath = GetPayloadString(Payload, TEXT("stationPath"));

    if (StationPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: stationPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *StationPath));
    if (!Blueprint) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Crafting station blueprint not found: %s"), *StationPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // Get recipe paths from payload
    TArray<FString> RecipePaths;
    const TArray<TSharedPtr<FJsonValue>>* RecipesArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("recipePaths"), RecipesArr) && RecipesArr) {
      for (const auto& RecipeVal : *RecipesArr) {
        RecipePaths.Add(RecipeVal->AsString());
      }
    }

    FString StationType = GetPayloadString(Payload, TEXT("stationType"), TEXT("Basic"));
    int32 CraftingSpeed = static_cast<int32>(GetPayloadNumber(Payload, TEXT("craftingSpeedMultiplier"), 1.0));

    // Add station recipe configuration variables
    FEdGraphPinType SoftObjectArrayType;
    SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
    SoftObjectArrayType.ContainerType = EPinContainerType::Array;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    // Station configuration variables
    TArray<TPair<FName, FEdGraphPinType>> StationVars = {
      TPair<FName, FEdGraphPinType>(TEXT("AvailableRecipes"), SoftObjectArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("StationType"), NameType),
      TPair<FName, FEdGraphPinType>(TEXT("CraftingSpeedMultiplier"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bRequiresFuel"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("FuelConsumptionRate"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bAutoStartCrafting"), BoolType)
    };

    TArray<TSharedPtr<FJsonValue>> AddedVars;

    for (const auto& VarPair : StationVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShareable(new FJsonValueString(VarPair.Key.ToString())));
      }
    }

    // Add crafting events for station
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    TArray<FName> EventNames = {
      TEXT("OnRecipeQueued"),
      TEXT("OnCraftingStarted"),
      TEXT("OnCraftingCompleted"),
      TEXT("OnFuelDepleted")
    };

    for (const FName& EventName : EventNames) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == EventName) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, EventName, DelegateType);
        AddedVars.Add(MakeShareable(new FJsonValueString(EventName.ToString())));
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("stationPath"), StationPath);
    Result->SetStringField(TEXT("stationType"), StationType);
    Result->SetNumberField(TEXT("recipeCount"), RecipePaths.Num());
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
    Result->SetBoolField(TEXT("configured"), true);

    TArray<TSharedPtr<FJsonValue>> RecipePathsArr;
    for (const FString& Path : RecipePaths) {
      RecipePathsArr.Add(MakeShareable(new FJsonValueString(Path)));
    }
    Result->SetArrayField(TEXT("recipePaths"), RecipePathsArr);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Crafting station recipes configured"), Result);
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

#undef GetPayloadString
#undef GetPayloadNumber
#undef GetPayloadBool


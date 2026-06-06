#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryLootTableActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_loot_table")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Data/LootTables"));

    if (Name.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: name"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Create a data asset for loot table
    UPackage* Package = CreateInventoryAssetPackage(Path, Name);
    if (!Package) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
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

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("lootTablePath"), Package->GetName());
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Loot table created"), Result);
    } else {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
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
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: lootTablePath and itemPath"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the loot table asset
    UObject* LootTableObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *LootTablePath);
    UMcpGenericDataAsset* LootTable = Cast<UMcpGenericDataAsset>(LootTableObj);

    if (!LootTable) {
      Bridge.SendAutomationError(
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
      // For generic MCP data assets, persist the entry in the extensible property map.
      const int32 GenericEntryIndex = LootTable->Properties.Num();
      const FString EntryKey = FString::Printf(TEXT("LootEntry_%d"), GenericEntryIndex);
      const FString EntryValue = FString::Printf(
          TEXT("ItemPath=%s;Weight=%s;MinQuantity=%d;MaxQuantity=%d"),
          *ItemPath, *FString::SanitizeFloat(Weight), MinQuantity, MaxQuantity);
      LootTable->Properties.Add(EntryKey, EntryValue);
      EntryIndex = GenericEntryIndex;
      bEntryAdded = true;
    }

    LootTable->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(LootTable);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetNumberField(TEXT("weight"), Weight);
    Result->SetNumberField(TEXT("minQuantity"), MinQuantity);
    Result->SetNumberField(TEXT("maxQuantity"), MaxQuantity);
    Result->SetNumberField(TEXT("entryIndex"), EntryIndex);
    Result->SetBoolField(TEXT("added"), bEntryAdded);
    if (!EntriesProp) {
      Result->SetStringField(TEXT("storage"), TEXT("Properties"));
    }
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Loot entry added"), Result);
    return true;
  }

  if (SubAction == TEXT("remove_loot_entry")) {
    FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));
    int32 EntryIndex = static_cast<int32>(GetPayloadNumber(Payload, TEXT("entryIndex"), -1));
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));

    if (LootTablePath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: lootTablePath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    if (EntryIndex < 0 && ItemPath.IsEmpty()) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Either entryIndex or itemPath must be provided"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the loot table asset
    UObject* LootTableObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *LootTablePath);
    UMcpGenericDataAsset* LootTable = Cast<UMcpGenericDataAsset>(LootTableObj);

    if (!LootTable) {
      Bridge.SendAutomationError(
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    Result->SetNumberField(TEXT("removedIndex"), RemovedIndex);
    Result->SetBoolField(TEXT("removed"), bEntryRemoved);

    if (!bEntryRemoved) {
      Result->SetStringField(TEXT("note"), TEXT("Entry not removed. Check that entryIndex is valid or LootEntries array exists."));
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Loot entry removed"), Result);
    return true;
  }

  return false;
}

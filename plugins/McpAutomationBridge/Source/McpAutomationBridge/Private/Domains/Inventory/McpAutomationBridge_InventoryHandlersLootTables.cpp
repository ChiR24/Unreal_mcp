#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

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
      Result->SetStringField(TEXT("assetPath"), Package->GetName() + TEXT(".") + FPackageName::GetShortName(Package->GetName())); // dogfood #55: consistent object path
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

    FProperty* EntriesProp = LootTable->GetClass()->FindPropertyByName(TEXT("LootEntries"));
    if (!EntriesProp) {
      EntriesProp = LootTable->GetClass()->FindPropertyByName(TEXT("Entries"));
    }

    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(EntriesProp)) {
      // For custom loot table classes with proper array properties
      FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(LootTable));
      int32 NewIdx = ArrayHelper.AddValue();
      if (NewIdx != INDEX_NONE) {
        EntryIndex = NewIdx;
        bEntryAdded = true;
        // Note: The new element's inner fields (item path, weight, quantities)
        // would need to be populated via reflection based on the struct definition
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

    // Generic loot tables keep their entries in the Properties map
    // (LootEntry_<n> = "ItemPath=...;Weight=..."), which the array path above
    // never sees; match by index key or by the ItemPath fragment.
    if (!bEntryRemoved) {
      TArray<FString> KeysToRemove;
      for (const TPair<FString, FString>& Pair : LootTable->Properties) {
        if (!Pair.Key.StartsWith(TEXT("LootEntry_"))) {
          continue;
        }
        const bool bIndexMatch = EntryIndex >= 0 &&
            Pair.Key.Equals(FString::Printf(TEXT("LootEntry_%d"), EntryIndex));
        const bool bItemMatch = !ItemPath.IsEmpty() &&
            (Pair.Value.Contains(FString::Printf(TEXT("ItemPath=%s;"), *ItemPath)) ||
             Pair.Value.EndsWith(FString::Printf(TEXT("ItemPath=%s"), *ItemPath)));
        if (bIndexMatch || bItemMatch) {
          KeysToRemove.Add(Pair.Key);
        }
      }
      for (const FString& Key : KeysToRemove) {
        LootTable->Properties.Remove(Key);
        bEntryRemoved = true;
        FString IndexText = Key;
        IndexText.RemoveFromStart(TEXT("LootEntry_"));
        RemovedIndex = FCString::Atoi(*IndexText);
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    Result->SetNumberField(TEXT("removedIndex"), RemovedIndex);
    Result->SetBoolField(TEXT("removed"), bEntryRemoved);

    if (!bEntryRemoved) {
      // Reporting success here hid a no-op (dogfood #51).
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             ItemPath.IsEmpty()
                                 ? FString::Printf(TEXT("No loot entry at index %d"), EntryIndex)
                                 : FString::Printf(TEXT("No loot entry references item %s"), *ItemPath),
                             Result, TEXT("NOT_FOUND"));
      return true;
    }

    LootTable->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(LootTable);
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Loot entry removed"), Result);
    return true;
  }

  return false;
}

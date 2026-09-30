#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryLootTableActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_loot_table")) {
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/Data/LootTables"));

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

      if (GetJsonBoolField(Payload, TEXT("save"), true)) {
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
    FString LootTablePath = GetJsonStringField(Payload, TEXT("lootTablePath"));
    FString ItemPath = GetJsonStringField(Payload, TEXT("itemPath"));
    double Weight = GetJsonNumberField(Payload, TEXT("lootWeight"), 1.0);
    int32 MinQuantity = static_cast<int32>(GetJsonNumberField(Payload, TEXT("minQuantity"), 1));
    int32 MaxQuantity = static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxQuantity"), 1));

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

    // Entries live in the generic asset's property map (LootEntry_<n> = "ItemPath=...;Weight=...").
    // n is one past the highest existing suffix: the map size repeats a live key after a removal.
    const int32 EntryIndex = NextIndexedPropertyIndex(LootTable->Properties, TEXT("LootEntry_"));
    LootTable->Properties.Add(FString::Printf(TEXT("LootEntry_%d"), EntryIndex),
        FString::Printf(TEXT("ItemPath=%s;Weight=%s;MinQuantity=%d;MaxQuantity=%d"),
                        *ItemPath, *FString::SanitizeFloat(Weight), MinQuantity, MaxQuantity));

    LootTable->MarkPackageDirty();

    if (GetJsonBoolField(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(LootTable);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetNumberField(TEXT("weight"), Weight);
    Result->SetNumberField(TEXT("minQuantity"), MinQuantity);
    Result->SetNumberField(TEXT("maxQuantity"), MaxQuantity);
    Result->SetNumberField(TEXT("entryIndex"), EntryIndex);
    Result->SetBoolField(TEXT("added"), true);
    Result->SetStringField(TEXT("storage"), TEXT("Properties"));
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Loot entry added"), Result);
    return true;
  }

  if (SubAction == TEXT("remove_loot_entry")) {
    FString LootTablePath = GetJsonStringField(Payload, TEXT("lootTablePath"));
    int32 EntryIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("entryIndex"), -1));
    FString ItemPath = GetJsonStringField(Payload, TEXT("itemPath"));

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

    // Entries live in the Properties map (LootEntry_<n> = "ItemPath=...;Weight=...");
    // match by index key or by the ItemPath fragment.
    {
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

    if (GetJsonBoolField(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(LootTable);
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Loot entry removed"), Result);
    return true;
  }

  return false;
}

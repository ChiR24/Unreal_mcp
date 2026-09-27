#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryLootDropActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("set_loot_quality_tiers")) {
    FString LootTablePath = GetJsonStringField(Payload, TEXT("lootTablePath"));

    if (LootTablePath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: lootTablePath"),
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

    if (Tiers.Num() == 0) {
      Tiers = {
        TPair<FString, double>(TEXT("Common"), 60.0),
        TPair<FString, double>(TEXT("Uncommon"), 25.0),
        TPair<FString, double>(TEXT("Rare"), 10.0),
        TPair<FString, double>(TEXT("Epic"), 4.0),
        TPair<FString, double>(TEXT("Legendary"), 1.0)
      };
    }

    // One "Name=Weight" entry per tier, comma-separated, like the other
    // inventory fields kept in the generic asset's Properties map.
    TArray<FString> Encoded;
    for (const auto& TierPair : Tiers) {
      Encoded.Add(FString::Printf(TEXT("%s=%s"), *TierPair.Key, *FString::SanitizeFloat(TierPair.Value)));
    }
    LootTable->Modify();
    LootTable->Properties.Add(TEXT("QualityTiers"), FString::Join(Encoded, TEXT(",")));
    LootTable->MarkPackageDirty();

    if (GetJsonBoolField(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(LootTable);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("lootTablePath"), LootTablePath);

    TArray<TSharedPtr<FJsonValue>> ConfiguredTiers;
    for (const auto& TierPair : Tiers) {
      TSharedPtr<FJsonObject> TierObj = McpHandlerUtils::CreateResultObject();
      TierObj->SetStringField(TEXT("name"), TierPair.Key);
      TierObj->SetNumberField(TEXT("dropWeight"), TierPair.Value);
      ConfiguredTiers.Add(MakeShared<FJsonValueObject>(TierObj));
    }
    Result->SetArrayField(TEXT("tiersConfigured"), ConfiguredTiers);
    Result->SetNumberField(TEXT("tierCount"), Tiers.Num());
    Result->SetBoolField(TEXT("configured"), true);


    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Quality tiers configured"), Result);
    return true;
  }

  return false;
}

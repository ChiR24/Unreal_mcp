#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpInventoryShared.h"

bool HandleInventoryLootDropActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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

    // A malformed tier used to be skipped, and a list of nothing but those became the five default tiers.
    TArray<TPair<FString, double>> Tiers;
    const TArray<TSharedPtr<FJsonValue>>* TiersArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("tiers"), TiersArr) && TiersArr) {
      if (TiersArr->Num() == 0) {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("tiers is empty; omit it for the five default tiers. Nothing was changed."), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      for (int32 Index = 0; Index < TiersArr->Num(); ++Index) {
        const TSharedPtr<FJsonObject>* TierObj = nullptr;
        const bool bObject = (*TiersArr)[Index].IsValid() && (*TiersArr)[Index]->TryGetObject(TierObj) && TierObj && (*TierObj).IsValid();
        const FString TierName = bObject ? GetJsonStringField(*TierObj, TEXT("name")).TrimStartAndEnd() : FString();
        double TierWeight = -1.0;
        if (bObject) {
          (*TierObj)->TryGetNumberField(TEXT("dropWeight"), TierWeight);
        }
        // The tiers are stored as Name=Weight pairs joined by commas, so neither character can be in a name.
        if (TierName.IsEmpty() || TierName.Contains(TEXT(",")) || TierName.Contains(TEXT("=")) || TierWeight < 0.0) {
          Bridge.SendAutomationError(RequestingSocket, RequestId,
              FString::Printf(TEXT("tiers[%d] needs a name without commas or equals signs and a dropWeight of 0 or more; nothing was changed."), Index),
              TEXT("INVALID_ARGUMENT"));
          return true;
        }
        Tiers.Add(TPair<FString, double>(TierName, TierWeight));
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

    const bool bSaved = GetJsonBoolField(Payload, TEXT("save"), true) && McpSafeAssetSave(LootTable);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("saved"), bSaved);
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


    McpHandlerUtils::AddVerification(Result, LootTable);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Quality tiers configured"), Result);
    return true;
  }

  return false;
}

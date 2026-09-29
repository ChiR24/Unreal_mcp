#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

UPackage* CreateValidatedInventoryAssetPackage(const FString& Path, const FString& Name, FString& OutError)
{
  FString PackageName;
  FString SanitizedName = SanitizeAssetName(Name);

  if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, OutError)) {
    return nullptr;
  }

  return CreatePackage(*PackageName);
}

UPackage* CreateInventoryAssetPackage(const FString& Path, const FString& Name)
{
  FString PackagePath = Path.IsEmpty() ? TEXT("/Game/Items") : Path;

  FString PackageName;
  FString PathError;
  FString SanitizedName = SanitizeAssetName(Name);
  if (!ValidateAssetCreationPath(PackagePath, SanitizedName, PackageName, PathError)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("CreateAssetPackage: %s"), *PathError);
    return nullptr;
  }

  return CreatePackage(*PackageName);
}

int32 NextIndexedPropertyIndex(const TMap<FString, FString>& Properties, const FString& Prefix)
{
  int32 Next = 0;
  for (const TPair<FString, FString>& Pair : Properties) {
    if (!Pair.Key.StartsWith(Prefix)) {
      continue;
    }
    // Only a plain run of digits names an index; a key such as "LootEntry_x" or "LootEntry_-1" is ignored.
    const FString Suffix = Pair.Key.RightChop(Prefix.Len());
    bool bIndex = Suffix.Len() > 0 && Suffix.Len() < 10;
    for (int32 Char = 0; bIndex && Char < Suffix.Len(); ++Char) {
      bIndex = FChar::IsDigit(Suffix[Char]);
    }
    if (bIndex) {
      Next = FMath::Max(Next, FCString::Atoi(*Suffix) + 1);
    }
  }
  return Next;
}

UBlueprint* LoadInventoryBlueprintOrError(UMcpAutomationBridgeSubsystem& Bridge,
                                          const FString& RequestId,
                                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                          const FString& BlueprintPath)
{
  if (BlueprintPath.IsEmpty()) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("Missing required parameter: blueprintPath"),
                               TEXT("MISSING_PARAMETER"));
    return nullptr;
  }

  FString Normalized;
  FString LoadError;
  UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, Normalized, LoadError);
  if (!Blueprint) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return nullptr;
  }

  return Blueprint;
}

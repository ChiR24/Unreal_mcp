#include "Domains/Inventory/McpInventoryShared.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

UPackage* CreateInventoryAssetPackage(UEngineWireSubsystem& Bridge, const FString& RequestId,
                                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString& Path,
                                      const FString& Name)
{
  FString PackageName;
  FString Error;
  const FString SanitizedName = SanitizeAssetName(Name);
  const FString Folder = Path.IsEmpty() ? FString(TEXT("/Game/Items")) : Path;
  if (!ValidateAssetCreationPath(Folder, SanitizedName, PackageName, Error)) {
    Bridge.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PACKAGE_CREATE_FAILED"));
    return nullptr;
  }
  // CreatePackage hands back a package already holding an asset, and the NewObject or factory after it replaced
  // that asset (its entries, ingredients and fields gone) or, for another class, brought the editor down.
  if (McpAssetExists(PackageName) || FindObject<UObject>(nullptr, *(PackageName + TEXT(".") + SanitizedName))) {
    Bridge.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s already exists; pick another name or folder, or change it with the configure actions. Nothing was created."), *PackageName),
                               TEXT("ASSET_ALREADY_EXISTS"));
    return nullptr;
  }
  UPackage* Package = McpCreatePackageOrNull(PackageName);
  if (!Package) {
    Bridge.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Could not create the package %s."), *PackageName),
                               TEXT("PACKAGE_CREATE_FAILED"));
  }
  return Package;
}

UMcpGenericDataAsset* NewInventoryDataAsset(UPackage* Package, const FString& Name, const TSharedPtr<FJsonObject>& Payload, bool& OutSaved,
                                            TFunctionRef<void(UMcpGenericDataAsset&)> Fill)
{
  UMcpGenericDataAsset* Asset = NewObject<UMcpGenericDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);
  OutSaved = false;
  if (Asset) {
    Fill(*Asset);
    Asset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Asset);
    OutSaved = GetJsonBoolField(Payload, TEXT("save"), true) && McpSafeAssetSave(Asset);
  }
  return Asset;
}

bool RequireReferencedAsset(UEngineWireSubsystem& Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                            const FString& Path, const TCHAR* Field)
{
  if (McpAssetExists(Path)) {
    return true;
  }
  Bridge.SendAutomationError(RequestingSocket, RequestId,
      FString::Printf(TEXT("%s '%s' names no asset; nothing was changed."), Field, *Path), TEXT("ASSET_NOT_FOUND"));
  return false;
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

UBlueprint* LoadInventoryBlueprintOrError(UEngineWireSubsystem& Bridge,
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

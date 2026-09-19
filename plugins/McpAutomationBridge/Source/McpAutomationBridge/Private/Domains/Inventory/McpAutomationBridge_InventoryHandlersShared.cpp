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

  UBlueprint* Blueprint =
      Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
  if (!Blueprint) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
        TEXT("BLUEPRINT_NOT_FOUND"));
    return nullptr;
  }

  return Blueprint;
}

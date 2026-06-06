#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

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

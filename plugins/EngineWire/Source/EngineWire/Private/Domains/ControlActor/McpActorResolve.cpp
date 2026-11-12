#include "Domains/ControlActor/McpActorSupport.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"

UMaterialInterface *LoadMaterialForMcp(const FString &MaterialPath,
                                       FString &OutResolvedPath,
                                       FString &OutError) {
  OutResolvedPath.Empty();
  OutError.Empty();

  const FString SafeMaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (SafeMaterialPath.IsEmpty()) {
    OutError = McpPathRefusalMessage(TEXT("materialPath"), MaterialPath);
    return nullptr;
  }

  OutResolvedPath = SafeMaterialPath;
  UObject *Loaded = McpLoadAsset(SafeMaterialPath);
  UMaterialInterface *Material = Cast<UMaterialInterface>(Loaded);
  if (!Material) {
    Material = LoadObject<UMaterialInterface>(nullptr, *SafeMaterialPath);
  }
  if (!Material && !SafeMaterialPath.Contains(TEXT("."))) {
    const FString ObjectPath = FString::Printf(
        TEXT("%s.%s"), *SafeMaterialPath,
        *FPaths::GetBaseFilename(SafeMaterialPath));
    Material = LoadObject<UMaterialInterface>(nullptr, *ObjectPath);
    if (Material) {
      OutResolvedPath = ObjectPath;
    }
  }

  if (!Material) {
    OutError = FString::Printf(TEXT("Material not found: %s"), *MaterialPath);
  }
  return Material;
}

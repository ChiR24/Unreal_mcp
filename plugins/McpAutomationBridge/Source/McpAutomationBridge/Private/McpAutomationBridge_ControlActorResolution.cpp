#include "McpAutomationBridge_ControlActorSupport.h"

#if WITH_EDITOR
UMaterialInterface *LoadMaterialForMcp(const FString &MaterialPath,
                                       FString &OutResolvedPath,
                                       FString &OutError) {
  OutResolvedPath.Empty();
  OutError.Empty();

  const FString SafeMaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (SafeMaterialPath.IsEmpty()) {
    OutError = TEXT("Invalid materialPath");
    return nullptr;
  }

  OutResolvedPath = SafeMaterialPath;
  UObject *Loaded = UEditorAssetLibrary::LoadAsset(SafeMaterialPath);
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

AActor *FindActorByNameInWorldForMcp(UWorld *World, const FString &Target,
                                     bool bExactMatchOnly) {
  if (!World || Target.IsEmpty()) {
    return nullptr;
  }

  TArray<AActor *> FuzzyMatches;
  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Actor = *It;
    if (!Actor) {
      continue;
    }

    if (Actor->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
        Actor->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
        Actor->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) {
      return Actor;
    }

    if (!bExactMatchOnly &&
        Actor->GetActorLabel().Contains(Target, ESearchCase::IgnoreCase)) {
      FuzzyMatches.Add(Actor);
    }
  }

  return FuzzyMatches.Num() == 1 ? FuzzyMatches[0] : nullptr;
}
#endif

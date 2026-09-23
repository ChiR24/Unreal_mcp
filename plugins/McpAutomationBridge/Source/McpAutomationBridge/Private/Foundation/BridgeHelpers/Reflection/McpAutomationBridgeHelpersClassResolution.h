#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#endif

static inline UClass *ResolveUClass(const FString &Input) {
  if (Input.IsEmpty())
    return nullptr;

  UClass *Found = FindObject<UClass>(nullptr, *Input);
  if (Found)
    return Found;

  // Only a path can be loaded: LoadObject on a short name ("AudioComponent")
  // always failed and logged "Failed to find object 'Class AudioComponent'"
  // before the /Script probe below found the class anyway.
  if (Input.Contains(TEXT("/"))) {
    Found = LoadObject<UClass>(nullptr, *Input);
    if (Found)
      return Found;
  }

  if (Input.EndsWith(TEXT("_C"))) {
    return nullptr;
  }

  const TArray<FString> ScriptPackages = {
      TEXT("/Script/Engine"),       TEXT("/Script/CoreUObject"),
      TEXT("/Script/UMG"),          TEXT("/Script/AIModule"),
      TEXT("/Script/NavigationSystem"), TEXT("/Script/Niagara")};

  // Native classes are registered as soon as their module loads, so FindObject
  // is enough; a LoadObject per package only logged one failure per miss.
  for (const FString &Pkg : ScriptPackages) {
    const FString TryPath = FString::Printf(TEXT("%s.%s"), *Pkg, *Input);
    Found = FindObject<UClass>(nullptr, *TryPath);
    if (Found)
      return Found;
  }

  for (TObjectIterator<UClass> It; It; ++It) {
    if (It->GetName() == Input) {
      return *It;
    }
  }

  return nullptr;
}

#if WITH_EDITOR
static inline UClass *ResolveClassByName(const FString &ClassNameOrPath) {
  if (ClassNameOrPath.IsEmpty())
    return nullptr;

  // A '..._C' path names a Blueprint's GENERATED CLASS, not an asset. Handing
  // it to UEditorAssetLibrary logged "The AssetData '..._C' could not be found
  // in the Asset Registry" and returned null - a scary error for a path that
  // resolved fine one line later. Ask for the class directly, and load the
  // owning Blueprint (the path without the suffix) when it is not in memory.
  if (ClassNameOrPath.EndsWith(TEXT("_C"))) {
    if (UClass *Generated = FindObject<UClass>(nullptr, *ClassNameOrPath))
      return Generated;
    if (UObject *Asset = UEditorAssetLibrary::LoadAsset(ClassNameOrPath.LeftChop(2))) {
      if (UBlueprint *BP = Cast<UBlueprint>(Asset))
        return BP->GeneratedClass;
    }
    return LoadObject<UClass>(nullptr, *ClassNameOrPath);
  }

  if ((ClassNameOrPath.StartsWith(TEXT("/")) ||
       ClassNameOrPath.Contains(TEXT("/"))) &&
      !ClassNameOrPath.StartsWith(TEXT("/Script/"))) {
    UObject *Loaded = UEditorAssetLibrary::LoadAsset(ClassNameOrPath);
    if (Loaded) {
      if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
        return BP->GeneratedClass;
      if (UClass *C = Cast<UClass>(Loaded))
        return C;
    }
  }

  if (UClass *Direct = FindObject<UClass>(nullptr, *ClassNameOrPath))
    return Direct;

  if (!ClassNameOrPath.Contains(TEXT("/")) &&
      !ClassNameOrPath.Contains(TEXT("."))) {
    const FString EnginePath =
        FString::Printf(TEXT("/Script/Engine.%s"), *ClassNameOrPath);
    // FindObject only: native /Script/Engine classes are always registered, so
    // a LoadObject here could never find more -- it only logged "Failed to find
    // object 'Class /Script/Engine.X'" for every class of another module
    // (InputModifierNegate, ...) before the scan below resolved it anyway.
    if (UClass *EngineClass = FindObject<UClass>(nullptr, *EnginePath))
      return EngineClass;

    const FString UMGPath =
        FString::Printf(TEXT("/Script/UMG.%s"), *ClassNameOrPath);
    if (UClass *UMGClass = FindObject<UClass>(nullptr, *UMGPath))
      return UMGClass;
  }

  if (ClassNameOrPath.Equals(TEXT("NiagaraComponent"),
                             ESearchCase::IgnoreCase)) {
    if (UClass *NiagaraComp = FindObject<UClass>(
            nullptr, TEXT("/Script/Niagara.NiagaraComponent"))) {
      return NiagaraComp;
    }
  }

  UClass *BestMatch = nullptr;
  for (TObjectIterator<UClass> It; It; ++It) {
    UClass *C = *It;
    if (!C)
      continue;

    if (C->GetName().Equals(ClassNameOrPath, ESearchCase::IgnoreCase)) {
      if (C->GetPathName().StartsWith(TEXT("/Script/")))
        return C;
      if (!BestMatch)
        BestMatch = C;
    } else if (C->GetPathName().EndsWith(
                   FString::Printf(TEXT(".%s"), *ClassNameOrPath),
                   ESearchCase::IgnoreCase)) {
      if (!BestMatch)
        BestMatch = C;
    }
  }

  return BestMatch;
}
#endif

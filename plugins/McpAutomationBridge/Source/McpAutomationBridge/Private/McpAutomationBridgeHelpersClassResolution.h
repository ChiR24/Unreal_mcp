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

  Found = LoadObject<UClass>(nullptr, *Input);
  if (Found)
    return Found;

  if (Input.EndsWith(TEXT("_C"))) {
    return nullptr;
  }

  const TArray<FString> ScriptPackages = {
      TEXT("/Script/Engine"),       TEXT("/Script/CoreUObject"),
      TEXT("/Script/UMG"),          TEXT("/Script/AIModule"),
      TEXT("/Script/NavigationSystem"), TEXT("/Script/Niagara")};

  for (const FString &Pkg : ScriptPackages) {
    const FString TryPath = FString::Printf(TEXT("%s.%s"), *Pkg, *Input);
    Found = FindObject<UClass>(nullptr, *TryPath);
    if (Found)
      return Found;
    Found = LoadObject<UClass>(nullptr, *TryPath);
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
    if (UClass *EngineClass = FindObject<UClass>(nullptr, *EnginePath))
      return EngineClass;
    if (UClass *EngineClassLoaded =
            LoadObject<UClass>(nullptr, *EnginePath))
      return EngineClassLoaded;

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

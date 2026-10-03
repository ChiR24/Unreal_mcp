#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BlueprintCreation/McpAutomationBridge_BlueprintCreationHandlersPrivate.h"


#include "Factories/BlueprintFactory.h"
#include "Factories/BlueprintFunctionLibraryFactory.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/UObjectIterator.h"

namespace McpBlueprintCreationHandlers {
namespace {

UClass *ResolveExplicitParentClass(const FString &ParentClassSpec) {
  if (ParentClassSpec.IsEmpty()) {
    return nullptr;
  }
  if (ParentClassSpec.StartsWith(TEXT("/Script/"))) {
    return LoadClass<UObject>(nullptr, *ParentClassSpec);
  }

  UClass *ResolvedParent = FindObject<UClass>(nullptr, *ParentClassSpec);
  const bool bLooksPathLike = ParentClassSpec.Contains(TEXT("/")) ||
                              ParentClassSpec.Contains(TEXT("."));
  if (!ResolvedParent && bLooksPathLike) {
    ResolvedParent =
        StaticLoadClass(UObject::StaticClass(), nullptr, *ParentClassSpec);
  }
  if (!ResolvedParent && !bLooksPathLike) {
    const TArray<FString> PrefixGuesses = {
        FString::Printf(TEXT("/Script/Engine.%s"), *ParentClassSpec),
        FString::Printf(TEXT("/Script/GameFramework.%s"), *ParentClassSpec),
        FString::Printf(TEXT("/Script/CoreUObject.%s"), *ParentClassSpec)};
    for (const FString &Guess : PrefixGuesses) {
      UClass *Loaded = FindObject<UClass>(nullptr, *Guess);
      if (!Loaded) {
        Loaded = StaticLoadClass(UObject::StaticClass(), nullptr, *Guess);
      }
      if (Loaded) {
        ResolvedParent = Loaded;
        break;
      }
    }
  }
  if (!ResolvedParent) {
    for (TObjectIterator<UClass> It; It; ++It) {
      UClass *Candidate = *It;
      if (Candidate &&
          Candidate->GetName().Equals(ParentClassSpec,
                                      ESearchCase::IgnoreCase)) {
        ResolvedParent = Candidate;
        break;
      }
    }
  }
  return ResolvedParent;
}

// The loaded classes carrying the class name of Spec (the part after its last '.'), by path: the
// way to the right module when a parent class path names the wrong one.
FString ClassesNamedLikeForMcp(const FString &Spec) {
  FString ShortName = Spec;
  Spec.Split(TEXT("."), nullptr, &ShortName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
  TArray<FString> Paths;
  for (TObjectIterator<UClass> It; It; ++It) {
    if (It->GetName().Equals(ShortName, ESearchCase::IgnoreCase)) {
      Paths.Add(It->GetPathName());
    }
  }
  return FString::Join(Paths, TEXT(", "));
}

}

UFactory *CreateBlueprintFactory(const FRequestContext &Context, FString &OutError) {
  const FString NormalizedParentClassSpec =
      Context.ParentClassSpec.ToLower().Replace(TEXT(" "), TEXT(""));
  const bool bFunctionLibraryByParent =
      NormalizedParentClassSpec.EndsWith(TEXT("blueprintfunctionlibrary"));
  const FString LowerType = Context.BlueprintTypeSpec.ToLower();
  const bool bFunctionLibraryByType =
      LowerType == TEXT("functionlibrary") ||
      LowerType == TEXT("function_library") ||
      LowerType == TEXT("function library");

  UClass *ResolvedParent = ResolveExplicitParentClass(Context.ParentClassSpec);
  if (!ResolvedParent && bFunctionLibraryByParent) {
    ResolvedParent = UBlueprintFunctionLibrary::StaticClass();
  }
  if (!ResolvedParent && !Context.BlueprintTypeSpec.IsEmpty()) {
    if (LowerType == TEXT("actor")) {
      ResolvedParent = AActor::StaticClass();
    } else if (LowerType == TEXT("pawn")) {
      ResolvedParent = APawn::StaticClass();
    } else if (LowerType == TEXT("character")) {
      ResolvedParent = ACharacter::StaticClass();
    } else if (bFunctionLibraryByType) {
      ResolvedParent = UBlueprintFunctionLibrary::StaticClass();
    }
  }

  // A named parent that resolves to nothing used to become Actor without a word: a camera shake
  // asked for under the wrong module came out an Actor Blueprint, and "Blueprint created" said
  // only that its shake properties did not exist.
  if (!ResolvedParent && !Context.ParentClassSpec.IsEmpty()) {
    const FString Candidates = ClassesNamedLikeForMcp(Context.ParentClassSpec);
    OutError = FString::Printf(TEXT("Parent class '%s' was not found, so nothing was created. "), *Context.ParentClassSpec) +
               (Candidates.IsEmpty() ? FString(TEXT("Give a native class as /Script/Module.Class or a Blueprint class path ending in _C."))
                                     : FString::Printf(TEXT("A class with that name: %s."), *Candidates));
    return nullptr;
  }

  if (ResolvedParent == UBlueprintFunctionLibrary::StaticClass()) {
    UBlueprintFunctionLibraryFactory *Factory =
        NewObject<UBlueprintFunctionLibraryFactory>();
    Factory->ParentClass = UBlueprintFunctionLibrary::StaticClass();
    return Factory;
  }

  UBlueprintFactory *Factory = NewObject<UBlueprintFactory>();
  Factory->ParentClass = ResolvedParent ? ResolvedParent : AActor::StaticClass();
  return Factory;
}

}


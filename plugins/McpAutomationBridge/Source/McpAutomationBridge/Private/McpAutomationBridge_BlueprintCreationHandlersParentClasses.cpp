#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BlueprintCreationHandlersPrivate.h"

#if WITH_EDITOR

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

}

UFactory *CreateBlueprintFactory(const FRequestContext &Context) {
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

#endif

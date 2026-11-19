#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BlueprintCreation/McpBPCreatePrivate.h"


#include "Factories/BlueprintFactory.h"
#include "Factories/BlueprintFunctionLibraryFactory.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Interface.h"
#include "UObject/UObjectIterator.h"

namespace McpBlueprintCreationHandlers {
namespace {

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

  // blueprintType Interface makes a Blueprint Interface: functions only, which other Blueprints implement.
  if (LowerType == TEXT("interface") && Context.ParentClassSpec.IsEmpty()) {
    UBlueprintFactory *Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = UInterface::StaticClass();
#if MCP_HAS_BLUEPRINT_FACTORY_TYPE
    Factory->BlueprintType = BPTYPE_Interface;
#endif
    return Factory;
  }

  // A native class by name or path, a Blueprint by its class path, its asset path or its name. The resolver this
  // had of its own loaded three guessed /Script paths for a short name, logging a failure for each into a reply
  // that then succeeded, and refused a Blueprint asset path.
  UClass *ResolvedParent = ResolveClassByName(Context.ParentClassSpec);
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


#pragma once

#include "CoreMinimal.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Dom/JsonObject.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

#include "EditorAssetLibrary.h"

// The name a reply gives an actor, so that sending it back reaches that actor:
// its label, unless another actor in the world shares it (every unnamed spawn is
// labelled "Cube", and the resolver takes the first match), then its unique
// object name. Replies that said "Cube" x200 left nothing to address.
inline FString McpActorRef(const AActor *Actor) {
  const FString &Label = Actor->GetActorLabel();
  if (UWorld *World = Actor->GetWorld()) {
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (*It != Actor && It->GetActorLabel().Equals(Label, ESearchCase::IgnoreCase))
        return Actor->GetName();
    }
  }
  return Label;
}

static inline void AddActorVerification(TSharedPtr<FJsonObject> Response,
                                        AActor *Actor) {
  if (!Response || !Actor)
    return;

  // actorPath is the actor object path (addressable by inspect/control_actor); the owning
  // package rides as packagePath. It used to carry the package path under the actor name.
  Response->SetStringField(TEXT("actorPath"), Actor->GetPathName());
  if (Actor->GetPackage()) {
    Response->SetStringField(TEXT("packagePath"), Actor->GetPackage()->GetPathName());
  }
  Response->SetStringField(TEXT("actorName"), McpActorRef(Actor));
  Response->SetStringField(TEXT("actorGuid"),
                           Actor->GetActorGuid().ToString());
  Response->SetBoolField(TEXT("existsAfter"), true);
  Response->SetStringField(TEXT("actorClass"), Actor->GetClass()->GetName());
}

static inline void
AddComponentVerification(TSharedPtr<FJsonObject> Response,
                         USceneComponent *Component) {
  if (!Response || !Component)
    return;

  Response->SetStringField(TEXT("componentName"), Component->GetName());
  Response->SetStringField(TEXT("componentClass"),
                           Component->GetClass()->GetName());
  if (AActor *Owner = Component->GetOwner()) {
    Response->SetStringField(
        TEXT("ownerActorPath"),
        Owner->GetPackage() ? Owner->GetPackage()->GetPathName()
                            : Owner->GetPathName());
  }
}

static inline void AddAssetVerification(TSharedPtr<FJsonObject> Response,
                                        UObject *Asset) {
  if (!Response || !Asset)
    return;

  const FString AssetPath = Asset->GetPackage()
                                ? Asset->GetPackage()->GetPathName()
                                : Asset->GetPathName();
  Response->SetStringField(TEXT("assetPath"), AssetPath);
  Response->SetStringField(TEXT("assetName"), Asset->GetName());
  Response->SetBoolField(TEXT("existsAfter"), true);
  Response->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
}

static inline void
AddAssetVerificationNested(TSharedPtr<FJsonObject> Response,
                           const FString &FieldName, UObject *Asset) {
  if (!Response || !Asset)
    return;
  TSharedPtr<FJsonObject> VerificationObj = MakeShared<FJsonObject>();
  AddAssetVerification(VerificationObj, Asset);
  Response->SetObjectField(FieldName, VerificationObj);
}

static inline bool VerifyAssetExists(TSharedPtr<FJsonObject> Response,
                                     const FString &AssetPath) {
  const bool bExists = McpAssetExists(AssetPath);
  if (Response) {
    Response->SetStringField(TEXT("verifiedPath"), AssetPath);
    Response->SetBoolField(TEXT("existsAfter"), bExists);
  }
  return bExists;
}

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif

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
  Response->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
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
  const FString AssetPath = Asset->GetPackage()
                                ? Asset->GetPackage()->GetPathName()
                                : Asset->GetPathName();
  VerificationObj->SetStringField(TEXT("assetPath"), AssetPath);
  VerificationObj->SetStringField(TEXT("assetName"), Asset->GetName());
  VerificationObj->SetBoolField(TEXT("existsAfter"), true);
  VerificationObj->SetStringField(TEXT("assetClass"),
                                  Asset->GetClass()->GetName());
  Response->SetObjectField(FieldName, VerificationObj);
}

static inline bool VerifyAssetExists(TSharedPtr<FJsonObject> Response,
                                     const FString &AssetPath) {
  const bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);
  if (Response) {
    Response->SetStringField(TEXT("verifiedPath"), AssetPath);
    Response->SetBoolField(TEXT("existsAfter"), bExists);
  }
  return bExists;
}
#endif

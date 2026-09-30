#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsOpFields.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace McpBlueprintHandlers {

// add_scs_component takes meshPath/materialPath, but the batched operations[]
// form read neither, so the one call meant to build a whole prefab still needed
// a second non-batched call per mesh - and until the caller looked, produced a
// component with StaticMesh None. Same spellings as the single-op action, in
// one place, so add_component and modify_component in a batch both honour them.
//
// Returns what did not land as "part: reason", the shape McpScsPropertyBag::Apply's
// misses take: a path that does not load, and a component that takes no mesh or
// material, used to pass in silence. bApplied is set when a part did.
inline TArray<FString> ApplyScsTemplateAssets(UActorComponent *Template,
                                              const TSharedPtr<FJsonObject> &Op,
                                              bool &bApplied) {
  TArray<FString> Rejected;
  if (!Template || !Op.IsValid()) {
    return Rejected;
  }
  const FString MeshPath = ScsOpMeshPath(Op);
  if (!MeshPath.IsEmpty()) {
    if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(Template)) {
      if (UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath)) {
        // SetStaticMesh, not a raw property write: it is what fixes up the
        // body setup and material slots the new mesh brings with it.
        SMC->SetStaticMesh(Mesh);
        bApplied = true;
      } else {
        Rejected.Add(FString::Printf(TEXT("meshPath: '%s' is not a static mesh that loads"), *MeshPath));
      }
    } else if (USkeletalMeshComponent *SkMC =
                   Cast<USkeletalMeshComponent>(Template)) {
      if (USkeletalMesh *Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath)) {
        SkMC->SetSkeletalMesh(Mesh, true);
        bApplied = true;
      } else {
        Rejected.Add(FString::Printf(TEXT("meshPath: '%s' is not a skeletal mesh that loads"), *MeshPath));
      }
    } else {
      Rejected.Add(FString::Printf(TEXT("meshPath: a %s takes no mesh"), *Template->GetClass()->GetName()));
    }
  }
  const FString MaterialPath = ScsOpMaterialPath(Op);
  if (!MaterialPath.IsEmpty()) {
    UPrimitiveComponent *PC = Cast<UPrimitiveComponent>(Template);
    if (!PC) {
      Rejected.Add(FString::Printf(TEXT("materialPath: a %s takes no material"), *Template->GetClass()->GetName()));
    } else if (UMaterialInterface *Mat =
                   LoadObject<UMaterialInterface>(nullptr, *MaterialPath)) {
      PC->SetMaterial(0, Mat);
      bApplied = true;
    } else {
      Rejected.Add(FString::Printf(TEXT("materialPath: '%s' is not a material that loads"), *MaterialPath));
    }
  }
  return Rejected;
}

// A mesh or material given to a hidden component never shows, and the batch said
// nothing: the new look seemed to be ignored. Empty when the component draws.
inline FString McpScsHiddenHint(const UActorComponent *Template,
                                const FString &ComponentName) {
  const USceneComponent *Scene = Cast<USceneComponent>(Template);
  if (!Scene || (Scene->GetVisibleFlag() && !Scene->bHiddenInGame)) {
    return FString();
  }
  return FString::Printf(
      TEXT("'%s' is hidden (%s), so its new mesh or material will not show; add ")
      TEXT("properties {\"bVisible\": true, \"bHiddenInGame\": false} if it should."),
      *ComponentName, Scene->GetVisibleFlag() ? TEXT("bHiddenInGame true") : TEXT("bVisible false"));
}

} // namespace McpBlueprintHandlers

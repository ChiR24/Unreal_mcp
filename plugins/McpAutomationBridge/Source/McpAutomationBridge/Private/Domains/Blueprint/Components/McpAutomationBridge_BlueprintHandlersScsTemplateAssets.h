#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace McpBlueprintHandlers {

// add_scs_component takes meshPath/materialPath, but the batched operations[]
// form read neither, so the one call meant to build a whole prefab still needed
// a second non-batched call per mesh - and until the caller looked, produced a
// component with StaticMesh None. Same spellings as the single-op action, in
// one place, so add_component and modify_component in a batch both honour them.
inline bool ApplyScsTemplateAssets(UActorComponent *Template,
                                   const TSharedPtr<FJsonObject> &Op) {
  if (!Template || !Op.IsValid()) {
    return false;
  }
  bool bApplied = false;
  FString MeshPath;
  if (!Op->TryGetStringField(TEXT("meshPath"), MeshPath)) {
    if (!Op->TryGetStringField(TEXT("mesh_path"), MeshPath)) {
      Op->TryGetStringField(TEXT("staticMesh"), MeshPath);
    }
  }
  if (!MeshPath.IsEmpty()) {
    if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(Template)) {
      if (UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath)) {
        // SetStaticMesh, not a raw property write: it is what fixes up the
        // body setup and material slots the new mesh brings with it.
        SMC->SetStaticMesh(Mesh);
        bApplied = true;
      }
    } else if (USkeletalMeshComponent *SkMC =
                   Cast<USkeletalMeshComponent>(Template)) {
      if (USkeletalMesh *Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath)) {
        SkMC->SetSkeletalMesh(Mesh, true);
        bApplied = true;
      }
    }
  }
  FString MaterialPath;
  if (!Op->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    Op->TryGetStringField(TEXT("material_path"), MaterialPath);
  }
  if (!MaterialPath.IsEmpty()) {
    if (UPrimitiveComponent *PC = Cast<UPrimitiveComponent>(Template)) {
      if (UMaterialInterface *Mat =
              LoadObject<UMaterialInterface>(nullptr, *MaterialPath)) {
        PC->SetMaterial(0, Mat);
        bApplied = true;
      }
    }
  }
  return bApplied;
}

} // namespace McpBlueprintHandlers
#endif

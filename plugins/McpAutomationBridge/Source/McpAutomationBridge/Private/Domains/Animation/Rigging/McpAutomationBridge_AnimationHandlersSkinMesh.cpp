#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Safety/McpSafeOperations.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#if __has_include("GeometryScript/MeshAssetFunctions.h")
#define MCP_HAS_GEOMETRY_SCRIPT 1
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBoneWeightFunctions.h"
#include "UDynamicMesh.h"
#else
#define MCP_HAS_GEOMETRY_SCRIPT 0
#endif

namespace McpAnimationHandlers {
#if WITH_EDITOR

// A garment parented to an actor is still rigid: it holds its shape while the
// body underneath animates, so arms push through sleeves and the hem stays put
// while the legs move. Clothing has to be skinned to the same skeleton as the
// body to move with it. Transferring weights from the body mesh beats computing
// them fresh wherever the garment hugs the body, because it inherits exactly
// the deformation the body already has; smooth binding is the fallback for when
// there is no dressed mesh to copy from.
bool HandleAnimationSkinMeshToSkeletonAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
#if !MCP_HAS_GEOMETRY_SCRIPT
  Context.bSuccess = false;
  Context.Message = TEXT("Skinning needs the GeometryScripting plugin");
  Context.ErrorCode = TEXT("NOT_SUPPORTED");
  Context.Resp->SetStringField(TEXT("error"), Context.Message);
  return false;
#else
  const auto Fail = [&Context](const FString &Text, const TCHAR *Code) {
    Context.bSuccess = false;
    Context.Message = Text;
    Context.ErrorCode = Code;
    Context.Resp->SetStringField(TEXT("error"), Text);
  };

  FString StaticMeshPath;
  FString SkeletonPath;
  FString OutputPath;
  FString SourceSkeletalMeshPath;
  Payload->TryGetStringField(TEXT("staticMeshPath"), StaticMeshPath);
  Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);
  Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
  Payload->TryGetStringField(TEXT("sourceSkeletalMesh"), SourceSkeletalMeshPath);

  UStaticMesh *Garment = LoadObject<UStaticMesh>(nullptr, *StaticMeshPath);
  USkeleton *Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
  if (Garment == nullptr || Skeleton == nullptr || OutputPath.IsEmpty()) {
    Fail(TEXT("staticMeshPath, skeletonPath and outputPath are required"),
         TEXT("INVALID_ARGUMENT"));
    return false;
  }

  UDynamicMesh *Mesh = NewObject<UDynamicMesh>();
  EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
  UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
      Garment, Mesh, FGeometryScriptCopyMeshFromAssetOptions(),
      FGeometryScriptMeshReadLOD(), Outcome);
  if (Outcome != EGeometryScriptOutcomePins::Success) {
    Fail(FString::Printf(TEXT("Could not read geometry from %s"),
                         *StaticMeshPath),
         TEXT("MESH_READ_FAILED"));
    return false;
  }

  USkeletalMesh *Dressed =
      SourceSkeletalMeshPath.IsEmpty()
          ? nullptr
          : LoadObject<USkeletalMesh>(nullptr, *SourceSkeletalMeshPath);
  if (Dressed != nullptr) {
    UDynamicMesh *Body = NewObject<UDynamicMesh>();
    EGeometryScriptOutcomePins BodyOutcome = EGeometryScriptOutcomePins::Failure;
    UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(
        Dressed, Body, FGeometryScriptCopyMeshFromAssetOptions(),
        FGeometryScriptMeshReadLOD(), BodyOutcome);
    if (BodyOutcome == EGeometryScriptOutcomePins::Success) {
      UGeometryScriptLibrary_MeshBoneWeightFunctions::TransferBoneWeightsFromMesh(
          Body, Mesh, FGeometryScriptTransferBoneWeightsOptions());
    } else {
      Dressed = nullptr;
    }
  }
  if (Dressed == nullptr) {
    UGeometryScriptLibrary_MeshBoneWeightFunctions::ComputeSmoothBoneWeights(
        Mesh, Skeleton, FGeometryScriptSmoothBoneWeightsOptions());
  }

  UPackage *Package = CreatePackage(*OutputPath);
  const FString AssetName = FPackageName::GetShortName(OutputPath);
  USkeletalMesh *Skinned = NewObject<USkeletalMesh>(
      Package, USkeletalMesh::StaticClass(), FName(*AssetName),
      RF_Public | RF_Standalone | RF_Transactional);
  // Setting the Skeleton pointer alone leaves the mesh without a reference
  // skeleton, and SkeletalRender asserts NumRefBasesInvMatrix != 0 the moment
  // anything draws it -- including the Content Browser thumbnail, which takes
  // the editor down rather than failing the call.
  Skinned->SetSkeleton(Skeleton);
  Skinned->SetRefSkeleton(Skeleton->GetReferenceSkeleton());
  Skinned->CalculateInvRefMatrices();
  EGeometryScriptOutcomePins WriteOutcome = EGeometryScriptOutcomePins::Failure;
  FGeometryScriptCopyMeshToAssetOptions WriteOptions;
  WriteOptions.bEnableRecomputeNormals = true;
  WriteOptions.bEnableRecomputeTangents = true;
  UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToSkeletalMesh(
      Mesh, Skinned, WriteOptions, FGeometryScriptMeshWriteLOD(), WriteOutcome);
  if (WriteOutcome != EGeometryScriptOutcomePins::Success) {
    Fail(FString::Printf(TEXT("Could not write the skinned mesh to %s"),
                         *OutputPath),
         TEXT("MESH_WRITE_FAILED"));
    return false;
  }

  // The garment keeps the source materials; without them it renders as the
  // default checker and looks like a different failure than it is.
  for (const FStaticMaterial &Slot : Garment->GetStaticMaterials()) {
    Skinned->GetMaterials().Add(FSkeletalMaterial(Slot.MaterialInterface));
  }
  Skinned->CalculateInvRefMatrices();
  Skinned->PostEditChange();
  FAssetRegistryModule::AssetCreated(Skinned);
  Skinned->MarkPackageDirty();
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  if (bSave) {
    McpSafeOperations::McpSafeAssetSave(Skinned);
  }

  Context.bSuccess = true;
  Context.Message = TEXT("Mesh skinned to skeleton");
  Context.Resp->SetStringField(TEXT("assetPath"), Skinned->GetPathName());
  Context.Resp->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());
  Context.Resp->SetStringField(TEXT("weights"),
                               Dressed != nullptr ? TEXT("transferred")
                                                  : TEXT("smooth"));
  return false;
#endif
}
#endif
} // namespace McpAnimationHandlers

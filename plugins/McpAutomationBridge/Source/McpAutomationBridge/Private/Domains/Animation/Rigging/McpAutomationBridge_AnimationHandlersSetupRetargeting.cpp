#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Domains/Animation/Rigging/McpAutomationBridge_AnimationRetargetPipeline.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"

#if MCP_HAS_IKRIG_PIPELINE
#include "RetargetEditor/IKRetargetBatchOperation.h"
#endif

namespace McpAnimationHandlers {
#if WITH_EDITOR

namespace {
void Fail(FActionContext &Context, const FString &Text, const TCHAR *Code) {
  Context.bSuccess = false;
  Context.Message = Text;
  Context.ErrorCode = Code;
  Context.Resp->SetStringField(TEXT("error"), Text);
}

// LoadObject accepts a bare package path; the asset registry does not, and a
// caller naming /Game/Anims/A_Run the way every other action accepts it would
// otherwise be told its animation does not exist.
FString ToObjectPath(const FString &Path) {
  return Path.Contains(TEXT("."))
             ? Path
             : Path + TEXT(".") + FPackageName::GetShortName(Path);
}
USkeletalMesh *ResolveMesh(const TSharedPtr<FJsonObject> &Payload,
                           const TCHAR *Field, USkeleton *Skeleton) {
  FString MeshPath;
  Payload->TryGetStringField(Field, MeshPath);
  if (!MeshPath.IsEmpty()) {
    if (USkeletalMesh *Named = LoadObject<USkeletalMesh>(nullptr, *MeshPath)) {
      return Named;
    }
  }
#if MCP_HAS_IKRIG_PIPELINE
  return McpFindMeshForSkeleton(Skeleton);
#else
  return nullptr;
#endif
}
} // namespace

bool HandleAnimationSetupRetargetingAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
#if !MCP_HAS_IKRIG_PIPELINE
  Fail(Context, TEXT("Retargeting needs the IKRig and IKRigEditor modules"),
       TEXT("NOT_SUPPORTED"));
  return false;
#else
  FString SourceSkeletonPath;
  FString TargetSkeletonPath;
  Payload->TryGetStringField(TEXT("sourceSkeleton"), SourceSkeletonPath);
  Payload->TryGetStringField(TEXT("targetSkeleton"), TargetSkeletonPath);
  USkeleton *SourceSkeleton =
      LoadObject<USkeleton>(nullptr, *SourceSkeletonPath);
  USkeleton *TargetSkeleton =
      LoadObject<USkeleton>(nullptr, *TargetSkeletonPath);
  if (SourceSkeleton == nullptr || TargetSkeleton == nullptr) {
    Fail(Context, TEXT("Retargeting failed - source or target skeleton not found"),
         TEXT("ASSET_NOT_FOUND"));
    return false;
  }

  const TArray<TSharedPtr<FJsonValue>> *AssetsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("assets"), AssetsArray)) {
    Payload->TryGetArrayField(TEXT("retargetAssets"), AssetsArray);
  }
  if (AssetsArray == nullptr || AssetsArray->Num() == 0) {
    Fail(Context, TEXT("setup_retargeting requires at least one animation asset to retarget"),
         TEXT("MISSING_RETARGET_ASSETS"));
    return false;
  }

  // The rig is built from a mesh, so a skeleton with no mesh anywhere in the
  // project cannot be retargeted at all; say which side is missing one.
  USkeletalMesh *SourceMesh =
      ResolveMesh(Payload, TEXT("sourceMesh"), SourceSkeleton);
  USkeletalMesh *TargetMesh =
      ResolveMesh(Payload, TEXT("targetMesh"), TargetSkeleton);
  if (SourceMesh == nullptr || TargetMesh == nullptr) {
    Fail(Context, FString::Printf(
             TEXT("No SkeletalMesh found for the %s skeleton; pass %s"),
             SourceMesh == nullptr ? TEXT("source") : TEXT("target"),
             SourceMesh == nullptr ? TEXT("sourceMesh") : TEXT("targetMesh")),
         TEXT("MESH_NOT_FOUND"));
    return false;
  }

  FString SavePath;
  Payload->TryGetStringField(TEXT("savePath"), SavePath);
  if (SavePath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), SavePath);
  }
  if (SavePath.IsEmpty()) {
    SavePath = FPackageName::GetLongPackagePath(
        SourceSkeleton->GetOutermost()->GetName());
  }
  if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
    UEditorAssetLibrary::MakeDirectory(SavePath);
  }

  FString Suffix;
  Payload->TryGetStringField(TEXT("suffix"), Suffix);
  if (Suffix.IsEmpty()) {
    Suffix = TEXT("_Retargeted");
  }
  bool bOverwrite = false;
  Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);

  // A retargeter the caller already tuned by hand beats one built blind.
  FString RetargeterPath;
  Payload->TryGetStringField(TEXT("retargeterPath"), RetargeterPath);
  UIKRetargeter *Retargeter =
      RetargeterPath.IsEmpty()
          ? nullptr
          : LoadObject<UIKRetargeter>(nullptr, *RetargeterPath);
  FString BuildError;
  if (Retargeter == nullptr) {
    const FString RigPath = SavePath / TEXT("Rigs");
    UIKRigDefinition *SourceRig = McpBuildIKRig(
        SourceMesh, RigPath,
        FString::Printf(TEXT("IKR_%s"), *SourceMesh->GetName()), BuildError);
    UIKRigDefinition *TargetRig =
        SourceRig == nullptr
            ? nullptr
            : McpBuildIKRig(TargetMesh, RigPath,
                            FString::Printf(TEXT("IKR_%s"),
                                            *TargetMesh->GetName()),
                            BuildError);
    if (TargetRig != nullptr) {
      Retargeter = McpBuildRetargeter(
          SourceRig, TargetRig, RigPath,
          FString::Printf(TEXT("RTG_%s_to_%s"), *SourceMesh->GetName(),
                          *TargetMesh->GetName()),
          BuildError);
    }
  }
  if (Retargeter == nullptr) {
    Fail(Context,
         BuildError.IsEmpty() ? TEXT("Could not build an IK Retargeter")
                              : BuildError,
         TEXT("RETARGETER_BUILD_FAILED"));
    return false;
  }

  IAssetRegistry &Registry =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")
          .Get();
  FIKRetargetBatchOperationInputs Inputs;
  TArray<TSharedPtr<FJsonValue>> Warnings;
  for (const TSharedPtr<FJsonValue> &Value : *AssetsArray) {
    if (!Value.IsValid() || Value->Type != EJson::String) {
      continue;
    }
    const FString AssetPath = Value->AsString();
    const FAssetData Data =
        Registry.GetAssetByObjectPath(FSoftObjectPath(ToObjectPath(AssetPath)), false);
    if (!Data.IsValid()) {
      Warnings.Add(MakeShared<FJsonValueString>(
          FString::Printf(TEXT("Not found, skipped: %s"), *AssetPath)));
      continue;
    }
    Inputs.AssetsToRetarget.Add(Data);
  }
  if (Inputs.AssetsToRetarget.Num() == 0) {
    Fail(Context, TEXT("None of the named animation assets exist"),
         TEXT("ASSET_NOT_FOUND"));
    return false;
  }

  Inputs.SourceMesh = SourceMesh;
  Inputs.TargetMesh = TargetMesh;
  Inputs.IKRetargetAsset = Retargeter;
  Inputs.Suffix = Suffix;
  Inputs.TargetPath = SavePath;
  Inputs.bUseSourcePath = false;
  Inputs.bIncludeReferencedAssets = false;
  Inputs.bOverwriteExistingFiles = bOverwrite;
  const TArray<FAssetData> Created =
      UIKRetargetBatchOperation::RunBatchRetarget(Inputs);

  TArray<TSharedPtr<FJsonValue>> RetargetedArray;
  for (const FAssetData &Data : Created) {
    RetargetedArray.Add(
        MakeShared<FJsonValueString>(Data.GetObjectPathString()));
  }
  Context.bSuccess = RetargetedArray.Num() > 0;
  Context.Message = Context.bSuccess
                        ? TEXT("Retargeting completed")
                        : TEXT("Retargeting produced no assets");
  if (!Context.bSuccess) {
    Context.ErrorCode = TEXT("NO_ASSETS_RETARGETED");
    Context.Resp->SetStringField(TEXT("error"), Context.Message);
  } else {
    Context.Resp->SetArrayField(TEXT("retargetedAssets"), RetargetedArray);
    if (UObject *First = Created[0].GetAsset()) {
      McpHandlerUtils::AddVerification(Context.Resp, First);
    }
  }
  if (Warnings.Num() > 0) {
    Context.Resp->SetArrayField(TEXT("warnings"), Warnings);
  }
  Context.Resp->SetStringField(TEXT("retargeter"), Retargeter->GetPathName());
  Context.Resp->SetStringField(TEXT("sourceSkeleton"),
                               SourceSkeleton->GetPathName());
  Context.Resp->SetStringField(TEXT("targetSkeleton"),
                               TargetSkeleton->GetPathName());
  return false;
#endif
}
#endif
} // namespace McpAnimationHandlers

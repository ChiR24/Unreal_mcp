#include "Domains/Animation/McpAnimActionContext.h"
#include "Domains/Animation/Rigging/McpAnimRetargetPipeline.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"

// FIKRetargetBatchOperationInputs and RunBatchRetarget are 5.8+; 5.6 and 5.7
// ship only the deprecated DuplicateAndRetarget, which cannot target a folder,
// so the bake is gated separately from the rig pipeline it sits on.
#if MCP_HAS_IKRIG_PIPELINE && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8
#define MCP_HAS_IKRIG_BATCH_RETARGET 1
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "Core/Compatibility/McpCompiledFeatures.h"
MCP_COMPILED_FEATURE(IKRIG_BATCH_RETARGET)
#else
#define MCP_HAS_IKRIG_BATCH_RETARGET 0
#endif

namespace McpAnimationHandlers {

namespace {
#if MCP_HAS_IKRIG_BATCH_RETARGET
USkeletalMesh *ResolveMesh(const TSharedPtr<FJsonObject> &Payload,
                           const TCHAR *Field, USkeleton *Skeleton) {
  FString MeshPath;
  Payload->TryGetStringField(Field, MeshPath);
  if (!MeshPath.IsEmpty()) {
    if (USkeletalMesh *Named = LoadObject<USkeletalMesh>(nullptr, *MeshPath)) {
      return Named;
    }
  }
  return McpFindMeshForSkeleton(Skeleton);
}
#endif
} // namespace

bool HandleAnimationSetupRetargetingAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
#if !MCP_HAS_IKRIG_BATCH_RETARGET
  Context.Fail(TEXT("NOT_SUPPORTED"),
               TEXT("Retargeting needs UE 5.8 or later with the IKRig and IKRigEditor "
                    "modules (the batch retarget API is 5.8+)"));
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
    Context.Fail(TEXT("ASSET_NOT_FOUND"), TEXT("Retargeting failed - source or target skeleton not found"));
    return false;
  }

  const TArray<TSharedPtr<FJsonValue>> *AssetsArray = nullptr;
  Payload->TryGetArrayField(TEXT("assets"), AssetsArray);
  if (AssetsArray == nullptr || AssetsArray->Num() == 0) {
    Context.Fail(TEXT("MISSING_RETARGET_ASSETS"), TEXT("setup_retargeting requires at least one animation asset to retarget"));
    return false;
  }

  // The rig is built from a mesh, so a skeleton with no mesh anywhere in the
  // project cannot be retargeted at all; say which side is missing one.
  USkeletalMesh *SourceMesh =
      ResolveMesh(Payload, TEXT("sourceMesh"), SourceSkeleton);
  USkeletalMesh *TargetMesh =
      ResolveMesh(Payload, TEXT("targetMesh"), TargetSkeleton);
  if (SourceMesh == nullptr || TargetMesh == nullptr) {
    Context.Fail(TEXT("MESH_NOT_FOUND"), FString::Printf(
             TEXT("No SkeletalMesh found for the %s skeleton; pass %s"),
             SourceMesh == nullptr ? TEXT("source") : TEXT("target"),
             SourceMesh == nullptr ? TEXT("sourceMesh") : TEXT("targetMesh")));
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
  // Trim, accept a filesystem spelling of a content folder, then run the
  // shared canonicalizer; a path that still is not a content folder is refused
  // rather than quietly redirected into the source skeleton's folder.
  SavePath.TrimStartAndEndInline();
  while (SavePath.Len() > 1 && SavePath.EndsWith(TEXT("/"))) {
    SavePath.LeftChopInline(1);
  }
  if (!FPackageName::IsValidLongPackageName(SavePath)) {
    FString Converted;
    SavePath = FPackageName::TryConvertFilenameToLongPackageName(SavePath, Converted)
                   ? Converted
                   : FString();
  }
  const FString SavePathAsGiven = SavePath;
  SavePath = SanitizeProjectRelativePath(SavePath);
  if (SavePath.IsEmpty()) {
    Context.Fail(TEXT("INVALID_PATH"), McpPathRefusalMessage(TEXT("savePath"), SavePathAsGiven));
    return false;
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
    Context.Fail(TEXT("RETARGETER_BUILD_FAILED"),
                 BuildError.IsEmpty() ? TEXT("Could not build an IK Retargeter") : BuildError);
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
    // The registry takes only an object path: a caller naming /Game/Anims/A_Run the way every other action accepts
    // it was told its animation does not exist.
    const FAssetData Data =
        Registry.GetAssetByObjectPath(FSoftObjectPath(McpToObjectPath(AssetPath)), false);
    if (!Data.IsValid()) {
      Warnings.Add(MakeShared<FJsonValueString>(
          FString::Printf(TEXT("Not found, skipped: %s"), *AssetPath)));
      continue;
    }
    Inputs.AssetsToRetarget.Add(Data);
  }
  if (Inputs.AssetsToRetarget.Num() == 0) {
    Context.Fail(TEXT("ASSET_NOT_FOUND"), TEXT("None of the named animation assets exist"));
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
    // The batch operation only marks the results dirty; without a save every
    // path reported below exists until the editor closes and not a moment
    // longer.
    if (UObject *Asset = Data.GetAsset()) {
      McpSafeOperations::McpSafeAssetSave(Asset);
    }
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
} // namespace McpAnimationHandlers

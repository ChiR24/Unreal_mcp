#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Domains/Animation/Assets/McpAutomationBridge_AnimationHandlersProceduralTracks.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"

#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "Animation/Skeleton.h"
#include "EditorAssetLibrary.h"
#include "Factories/PoseAssetFactory.h"

// create_pose_library: a real UPoseAsset built from a sequence, one pose per key frame, the asset
// PoseByName and PoseBlender nodes read. The factory takes the sequence's skeleton.
namespace McpAnimationHandlers {
bool HandleAnimationCreatePoseLibraryAction(FActionContext &Context, const TSharedPtr<FJsonObject> &Payload) {
  const FString Name = GetJsonStringField(Payload, TEXT("name"));
  const FString Folder = SanitizeProjectRelativePath(GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/Animations")));
  const FString SequencePath = GetJsonStringField(Payload, TEXT("sourceAnimationPath"));
  if (Name.IsEmpty() || Folder.IsEmpty()) {
    Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("name and a /Game folder in path are required for create_pose_library."));
    return false;
  }
  const FString AssetPath = Folder / SanitizeAssetName(Name);
  if (UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    Context.Fail(TEXT("ASSET_EXISTS"), FString::Printf(TEXT("'%s' already exists; pick another name or delete it first."), *AssetPath));
    return false;
  }
  UAnimSequence *Sequence = SequencePath.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *SequencePath, nullptr, LOAD_NoWarn);
  USkeleton *Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
  if (!Skeleton) {
    Context.Fail(TEXT("ANIMATION_NOT_FOUND"), FString::Printf(
        TEXT("No animation sequence with a skeleton at '%s'; sourceAnimationPath must name an AnimSequence."), *SequencePath));
    return false;
  }
  TArray<FString> PoseNames;
  const TArray<TSharedPtr<FJsonValue>> *NameValues = nullptr;
  if (Payload->TryGetArrayField(TEXT("poseNames"), NameValues)) {
    for (const TSharedPtr<FJsonValue> &Value : *NameValues) {
      const FString PoseName = Value.IsValid() ? Value->AsString().TrimStartAndEnd() : FString();
      if (PoseName.IsEmpty() || PoseNames.Contains(PoseName)) {
        Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("poseNames must be distinct, non-empty names, one per key frame."));
        return false;
      }
      PoseNames.Add(PoseName);
    }
  }
  UPoseAssetFactory *Factory = NewObject<UPoseAssetFactory>();
  Factory->SourceAnimation = Sequence;
  Factory->PoseNames = PoseNames;
  bool bExisting = false;
  FString Code;
  FString Error;
  UPoseAsset *PoseAsset = Cast<UPoseAsset>(CreateOrReuseAnimAsset(UPoseAsset::StaticClass(), Factory, Folder,
                                                                 SanitizeAssetName(Name), bExisting, Code, Error));
  if (!PoseAsset || PoseAsset->GetNumPoses() == 0) {
    if (PoseAsset) {
      McpSafeOperations::McpDeleteAssetAndFile(PoseAsset->GetPathName());
    }
    Context.Fail(PoseAsset ? TEXT("NO_POSES") : *Code, PoseAsset
        ? FString::Printf(TEXT("'%s' has no key frames to turn into poses; nothing was created."), *SequencePath) : Error);
    return false;
  }
  const bool bAdditive = GetJsonBoolField(Payload, TEXT("additive"), false);
  if (bAdditive && !PoseAsset->ConvertSpace(true, 0)) {
    McpSafeOperations::McpDeleteAssetAndFile(PoseAsset->GetPathName());
    Context.Fail(TEXT("ADDITIVE_FAILED"), TEXT("The poses could not be made additive against pose 0; nothing was created."));
    return false;
  }
  const bool bSaved = McpSafeAssetSave(PoseAsset);
  // Before 5.3 the pose names are curve names registered on the Skeleton; unsaved, they vanish on restart
  // and PoseByName finds nothing.
  bool bSkeletonSaved = false;
#if ENGINE_MINOR_VERSION < 3
  if (Skeleton->GetOutermost()->IsDirty()) {
    bSkeletonSaved = McpSafeAssetSave(Skeleton);
  }
#endif
  TArray<TSharedPtr<FJsonValue>> Names;
  for (int32 Index = 0; Index < PoseAsset->GetNumPoses(); ++Index) {
    Names.Add(MakeShared<FJsonValueString>(PoseAsset->GetPoseNameByIndex(Index).ToString()));
  }
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  Resp->SetStringField(TEXT("assetPath"), PoseAsset->GetPathName());
  Resp->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());
  Resp->SetNumberField(TEXT("poseCount"), PoseAsset->GetNumPoses());
  Resp->SetArrayField(TEXT("poseNames"), Names);
  Resp->SetBoolField(TEXT("additive"), PoseAsset->IsValidAdditive());
  Resp->SetBoolField(TEXT("saved"), bSaved);
  Resp->SetBoolField(TEXT("skeletonSaved"), bSkeletonSaved);
  McpHandlerUtils::AddVerification(Resp, PoseAsset);
  Context.bSuccess = true;
  Context.Message = FString::Printf(TEXT("Pose asset %s created with %d poses"), *PoseAsset->GetName(), PoseAsset->GetNumPoses());
  return false;
}
} // namespace McpAnimationHandlers

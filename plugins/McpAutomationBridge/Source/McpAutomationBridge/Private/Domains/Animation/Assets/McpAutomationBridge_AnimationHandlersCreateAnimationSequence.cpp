#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Factories/AnimSequenceFactory.h"
#include "Modules/ModuleManager.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateAnimationSequenceAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString SequenceName;
    if (!Payload->TryGetStringField(TEXT("name"), SequenceName) ||
        SequenceName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("sequenceName"), SequenceName);
    }

    if (SequenceName.IsEmpty()) {
      Message = TEXT("name or sequenceName required for create_animation_sequence");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        // Published contract names the folder `path`.
        Payload->TryGetStringField(TEXT("path"), SavePath);
      }
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      // A virtual bone whose source/target bone no longer exists makes the
      // engine assert (index -1) while compressing the new sequence; refuse
      // with a repairable error instead of taking the editor down.
      FString DanglingVirtualBone;
      if (TargetSkeleton) {
        const FReferenceSkeleton &RefSkeleton = TargetSkeleton->GetReferenceSkeleton();
        for (const FVirtualBone &VB : TargetSkeleton->GetVirtualBones()) {
          if (RefSkeleton.FindBoneIndex(VB.SourceBoneName) == INDEX_NONE ||
              RefSkeleton.FindBoneIndex(VB.TargetBoneName) == INDEX_NONE) {
            DanglingVirtualBone = VB.VirtualBoneName.ToString();
            break;
          }
        }
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath required for create_animation_sequence");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else if (!DanglingVirtualBone.IsEmpty()) {
        Message = FString::Printf(
            TEXT("Skeleton has virtual bone '%s' whose source or target bone does not exist; remove it with delete_virtual_bone before creating sequences"),
            *DanglingVirtualBone);
        ErrorCode = TEXT("SKELETON_INVALID_VIRTUAL_BONE");
        Resp->SetStringField(TEXT("error"), Message);
        Resp->SetStringField(TEXT("virtualBoneName"), DanglingVirtualBone);
      } else {
        if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
          UEditorAssetLibrary::MakeDirectory(SavePath);
        }

        UAnimSequenceFactory *SequenceFactory = NewObject<UAnimSequenceFactory>();
        if (!SequenceFactory) {
          Message = TEXT("Failed to create AnimSequence factory");
          ErrorCode = TEXT("FACTORY_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          SequenceFactory->TargetSkeleton = TargetSkeleton;

          FAssetToolsModule &AssetToolsModule =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
          UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
              SequenceName, SavePath, UAnimSequence::StaticClass(), SequenceFactory);

          if (!NewAsset) {
            Message = TEXT("Failed to create animation sequence");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            bSuccess = true;
            Message = TEXT("Animation sequence created successfully");
            Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
          }
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers

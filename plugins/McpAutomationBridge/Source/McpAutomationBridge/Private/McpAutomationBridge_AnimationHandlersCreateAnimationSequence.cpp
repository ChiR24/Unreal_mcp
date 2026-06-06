#include "McpAutomationBridge_AnimationHandlersActionContext.h"

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


    // Create a new animation sequence asset
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
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath required for create_animation_sequence");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
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

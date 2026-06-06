#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"

#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Modules/ModuleManager.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreatePoseLibraryAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Create a pose library asset
    FString LibraryName;
    if (!Payload->TryGetStringField(TEXT("name"), LibraryName) || LibraryName.IsEmpty()) {
      Message = TEXT("name required for create_pose_library");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        Payload->TryGetStringField(TEXT("path"), SavePath);
      }
      if (SavePath.IsEmpty()) {
        Payload->TryGetStringField(TEXT("directory"), SavePath);
      }
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations/PoseLibraries");
      }
      SavePath = SanitizeProjectRelativePath(SavePath.TrimStartAndEnd());
      if (SavePath.IsEmpty()) {
        Message = TEXT("Invalid savePath for create_pose_library");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      if (!SavePath.IsEmpty() && SkeletonPath.IsEmpty()) {
        Message = TEXT("skeletonPath required for create_pose_library");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else if (!SavePath.IsEmpty()) {
        USkeleton *TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
        if (!TargetSkeleton) {
          Message = FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
          ErrorCode = TEXT("ASSET_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
            UEditorAssetLibrary::MakeDirectory(SavePath);
          }

          // Create a Data Asset to serve as a pose library container
          FAssetToolsModule &AssetToolsModule =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
          UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
              LibraryName, SavePath, UMcpGenericDataAsset::StaticClass(), nullptr);

          if (NewAsset) {
            if (UMcpGenericDataAsset* PoseLibrary = Cast<UMcpGenericDataAsset>(NewAsset)) {
              PoseLibrary->ItemName = LibraryName;
              PoseLibrary->Description = TEXT("Pose Library for animation poses");
              PoseLibrary->Properties.Add(TEXT("SkeletonPath"), SkeletonPath);
              PoseLibrary->MarkPackageDirty();
              McpSafeOperations::McpSafeAssetSave(PoseLibrary);
            }

            bSuccess = true;
            Message = TEXT("Pose library created successfully");
            Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
            Resp->SetStringField(TEXT("savePath"), SavePath);
            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
            McpHandlerUtils::AddVerification(Resp, NewAsset);
          } else {
            Message = TEXT("Failed to create pose library asset");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          }
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers

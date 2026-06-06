#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridge_AnimationHandlersBlendSpaceAssets.h"
#include "McpHandlerUtils.h"

#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Modules/ModuleManager.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateAimOffsetAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Create an aim offset (2D by default)
    FString AimOffsetName;
    if (!Payload->TryGetStringField(TEXT("name"), AimOffsetName) ||
        AimOffsetName.IsEmpty()) {
      Message = TEXT("name required for create_aim_offset");
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
        Message = TEXT("Valid skeletonPath required for create_aim_offset");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
          UEditorAssetLibrary::MakeDirectory(SavePath);
        }

        // Check if 1D or 2D aim offset
        bool bIs1D = false;
        Payload->TryGetBoolField(TEXT("is1D"), bIs1D);

        UClass *AimOffsetClass = bIs1D ? UAimOffsetBlendSpace1D::StaticClass() : UAimOffsetBlendSpace::StaticClass();

        // Create using the appropriate factory
        UFactory *Factory = nullptr;
        if (bIs1D) {
          UBlendSpaceFactory1D *Factory1D = NewObject<UBlendSpaceFactory1D>();
          if (Factory1D) {
            Factory1D->TargetSkeleton = TargetSkeleton;
            Factory = Factory1D;
          }
        } else {
          UBlendSpaceFactoryNew *Factory2D = NewObject<UBlendSpaceFactoryNew>();
          if (Factory2D) {
            Factory2D->TargetSkeleton = TargetSkeleton;
            Factory = Factory2D;
          }
        }

        if (!Factory) {
          Message = TEXT("Failed to create aim offset factory");
          ErrorCode = TEXT("FACTORY_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          FAssetToolsModule &AssetToolsModule =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
          UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
              AimOffsetName, SavePath, AimOffsetClass, Factory);

          if (!NewAsset) {
            Message = TEXT("Failed to create aim offset");
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            // Apply axis configuration for aim offset (typically -90 to 90 for yaw/pitch)
            ApplyBlendSpaceConfiguration(NewAsset, Payload, !bIs1D);

            bSuccess = true;
            Message = TEXT("Aim offset created successfully");
            Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
            Resp->SetBoolField(TEXT("is1D"), bIs1D);
          }
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers

#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpHandlerUtils.h"

#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Factories/AnimMontageFactory.h"
#include "Modules/ModuleManager.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateMontageAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Create a new animation montage
    FString MontageName;
    if (!Payload->TryGetStringField(TEXT("name"), MontageName) ||
        MontageName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("montageName"), MontageName);
    }

    if (MontageName.IsEmpty()) {
      Message = TEXT("name or montageName required for create_montage");
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
        Message = TEXT("Valid skeletonPath required for create_montage");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        // Check if an asset already exists at the target path to prevent UObject class collision crash
        FString ObjectPath = FString::Printf(TEXT("%s/%s"), *SavePath, *MontageName);
        if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
        {
          UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
          if (ExistingAsset)
          {
            if (ExistingAsset->IsA<UAnimMontage>())
            {
              // Same type - return success with existing asset info
              bSuccess = true;
              Message = FString::Printf(TEXT("Animation montage '%s' already exists - reusing existing asset"), *MontageName);
              Resp->SetStringField(TEXT("assetPath"), ObjectPath);
              Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
              Resp->SetBoolField(TEXT("existingAsset"), true);
              McpHandlerUtils::AddVerification(Resp, ExistingAsset);
            }
            else
            {
              // Different type - return error to prevent crash
              FString ExistingClassName = ExistingAsset->GetClass()->GetName();
              Message = FString::Printf(
                TEXT("Cannot create AnimMontage: asset '%s' already exists as type '%s'"),
                *ObjectPath, *ExistingClassName);
              ErrorCode = TEXT("ASSET_TYPE_MISMATCH");
              Resp->SetStringField(TEXT("error"), Message);
              Resp->SetStringField(TEXT("existingClass"), ExistingClassName);
            }
          }
        }
        else {
          if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
            UEditorAssetLibrary::MakeDirectory(SavePath);
          }

          UAnimMontageFactory *MontageFactory = NewObject<UAnimMontageFactory>();
          if (!MontageFactory) {
            Message = TEXT("Failed to create AnimMontage factory");
            ErrorCode = TEXT("FACTORY_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            MontageFactory->TargetSkeleton = TargetSkeleton;

            FAssetToolsModule &AssetToolsModule =
                FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
            UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
                MontageName, SavePath, UAnimMontage::StaticClass(), MontageFactory);

            if (!NewAsset) {
              Message = TEXT("Failed to create animation montage");
              ErrorCode = TEXT("ASSET_CREATION_FAILED");
              Resp->SetStringField(TEXT("error"), Message);
            } else {
              bSuccess = true;
              Message = TEXT("Animation montage created successfully");
              Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
              Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
            }
          }
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers

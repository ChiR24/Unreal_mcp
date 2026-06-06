#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpHandlerUtils.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/AnimSequenceFactory.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateAnimationAssetAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString AssetName;
    if (!Payload->TryGetStringField(TEXT("name"), AssetName) ||
        AssetName.IsEmpty()) {
      Message = TEXT("name required for create_animation_asset");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }
      SavePath = SavePath.TrimStartAndEnd();

      if (!FPackageName::IsValidLongPackageName(SavePath)) {
        FString NormalizedPath;
        if (!FPackageName::TryConvertFilenameToLongPackageName(
                SavePath, NormalizedPath)) {
          Message = TEXT("Invalid savePath for animation asset");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
          SavePath.Reset();
        } else {
          SavePath = NormalizedPath;
        }
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);
      USkeleton *TargetSkeleton = nullptr;
      const bool bHadSkeletonPath = !SkeletonPath.IsEmpty();
      if (bHadSkeletonPath) {
        if (UEditorAssetLibrary::DoesAssetExist(SkeletonPath)) {
          TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
        }
      }

      if (!TargetSkeleton) {
        if (bHadSkeletonPath) {
          Message =
              FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
          ErrorCode = TEXT("ASSET_NOT_FOUND");
        } else {
          Message = TEXT("skeletonPath is required for create_animation_asset");
          ErrorCode = TEXT("INVALID_ARGUMENT");
        }

        Resp->SetStringField(TEXT("error"), Message);
      } else if (!SavePath.IsEmpty()) {
        if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
          UEditorAssetLibrary::MakeDirectory(SavePath);
        }

        FString AssetType;
        Payload->TryGetStringField(TEXT("assetType"), AssetType);
        AssetType = AssetType.ToLower();
        if (AssetType.IsEmpty()) {
          AssetType = TEXT("sequence");
        }

        UFactory *Factory = nullptr;
        UClass *DesiredClass = nullptr;
        FString AssetTypeString;

        if (AssetType == TEXT("montage")) {
          UAnimMontageFactory *MontageFactory =
              NewObject<UAnimMontageFactory>();
          if (MontageFactory) {
            MontageFactory->TargetSkeleton = TargetSkeleton;
            Factory = MontageFactory;
            DesiredClass = UAnimMontage::StaticClass();
            AssetTypeString = TEXT("Montage");
          }
        } else {
          UAnimSequenceFactory *SequenceFactory =
              NewObject<UAnimSequenceFactory>();
          if (SequenceFactory) {
            SequenceFactory->TargetSkeleton = TargetSkeleton;
            Factory = SequenceFactory;
            DesiredClass = UAnimSequence::StaticClass();
            AssetTypeString = TEXT("Sequence");
          }
        }

        if (!Factory || !DesiredClass) {
          Message = TEXT("Unsupported assetType for create_animation_asset");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          const FString ObjectPath =
              FString::Printf(TEXT("%s/%s"), *SavePath, *AssetName);
          if (UEditorAssetLibrary::DoesAssetExist(ObjectPath)) {
            bSuccess = true;
            Message =
                TEXT("Animation asset already exists - existing asset reused");
            Resp->SetStringField(TEXT("assetPath"), ObjectPath);
            Resp->SetStringField(TEXT("assetType"), AssetTypeString);
            Resp->SetBoolField(TEXT("existingAsset"), true);
            UObject* ExistingAsset = LoadObject<UObject>(nullptr, *ObjectPath);
            if (ExistingAsset) {
              McpHandlerUtils::AddVerification(Resp, ExistingAsset);
            }
          } else {
            FAssetToolsModule &AssetToolsModule =
                FModuleManager::LoadModuleChecked<FAssetToolsModule>(
                    "AssetTools");
            UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
                AssetName, SavePath, DesiredClass, Factory);

            if (!NewAsset) {
              Message = TEXT("Failed to create animation asset");
              ErrorCode = TEXT("ASSET_CREATION_FAILED");
              Resp->SetStringField(TEXT("error"), Message);
            } else {
              Resp->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
              Resp->SetStringField(TEXT("assetType"), AssetTypeString);
              Resp->SetBoolField(TEXT("existingAsset"), false);
              McpHandlerUtils::AddVerification(Resp, NewAsset);
              bSuccess = true;
              Message = FString::Printf(TEXT("Animation %s created"),
                                        *AssetTypeString);
            }
          }
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers

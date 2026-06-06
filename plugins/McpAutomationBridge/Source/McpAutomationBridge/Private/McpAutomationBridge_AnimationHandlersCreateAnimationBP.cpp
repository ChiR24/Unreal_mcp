#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpHandlerUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Modules/ModuleManager.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateAnimationBPAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Message = TEXT("name field required for animation blueprint creation");
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

      // Fallback: try meshPath if skeleton missing
      if (!TargetSkeleton) {
        FString MeshPath;
        if (Payload->TryGetStringField(TEXT("meshPath"), MeshPath) &&
            !MeshPath.IsEmpty()) {
          USkeletalMesh *Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
          if (Mesh) {
            TargetSkeleton = Mesh->GetSkeleton();
          }
        }
      }

      if (!TargetSkeleton) {
        Message =
            TEXT("Valid skeletonPath or meshPath required to find skeleton");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        UAnimBlueprintFactory *Factory = NewObject<UAnimBlueprintFactory>();
        if (!Factory) {
          Message = TEXT("Failed to create Animation Blueprint factory");
          ErrorCode = TEXT("FACTORY_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
        Factory->TargetSkeleton = TargetSkeleton;

        // Allow parent class override
        FString ParentClassPath;
        if (Payload->TryGetStringField(TEXT("parentClass"), ParentClassPath) &&
            !ParentClassPath.IsEmpty()) {
          UClass *ParentClass = LoadClass<UObject>(nullptr, *ParentClassPath);
          if (ParentClass) {
            Factory->ParentClass = ParentClass;
          }
        }

        FAssetToolsModule &AssetToolsModule =
            FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
            Name, SavePath, UAnimBlueprint::StaticClass(), Factory);

        if (NewAsset) {
          bSuccess = true;
          Message = TEXT("Animation Blueprint created");
          Resp->SetStringField(TEXT("blueprintPath"), NewAsset->GetPathName());
          Resp->SetStringField(TEXT("skeletonPath"),
                               TargetSkeleton->GetPathName());
          McpHandlerUtils::AddVerification(Resp, NewAsset);
        } else {
          Message = TEXT("Failed to create Animation Blueprint asset");
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

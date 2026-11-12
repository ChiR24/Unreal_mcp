#include "Domains/Animation/McpAnimActionContext.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Animation/Skeleton.h"

namespace McpAnimationHandlers {
bool HandleAnimationSetupIKAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString IKName;
    if (!Payload->TryGetStringField(TEXT("name"), IKName) || IKName.IsEmpty()) {
      Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("name field required for IK setup"));
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
      if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) ||
          SkeletonPath.IsEmpty()) {
        Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("skeletonPath is required to bind IK to a skeleton"));
      } else {
        USkeleton *TargetSkeleton =
            LoadObject<USkeleton>(nullptr, *SkeletonPath);
        if (!TargetSkeleton) {
          Context.Fail(TEXT("LOAD_FAILED"), TEXT("Failed to load skeleton for IK"));
        } else {
          FString FactoryError;
          UBlueprint *ControlRigBlueprint = nullptr;
          bool bExisting = false;
#if MCP_HAS_CONTROLRIG_FACTORY
          ControlRigBlueprint = McpAnimationAuthoring::CreateControlRigBlueprint(
              IKName, SavePath, TargetSkeleton, FactoryError, &bExisting);
#else
          FactoryError =
              TEXT("Control Rig factory not available in this editor build");
#endif
          if (!ControlRigBlueprint) {
            Context.Fail(TEXT("ASSET_CREATION_FAILED"), FactoryError.IsEmpty() ? TEXT("Failed to create IK asset")
                                             : FactoryError);
          } else {
            bSuccess = true;
            // A rig already at the path is handed back untouched: the reply said "created" for it.
            Message = bExisting ? TEXT("An IK Control Rig of that name already exists and was kept as it is")
                                : TEXT("IK setup created successfully");
            if (bExisting) {
              Resp->SetBoolField(TEXT("existingAsset"), true);
              McpHandlerUtils::MarkNoAssetsChanged(Resp);
            }
            const FString ControlRigPath = ControlRigBlueprint->GetPathName();
            Resp->SetStringField(TEXT("ikPath"), ControlRigPath);
            Resp->SetStringField(TEXT("controlRigPath"), ControlRigPath);
            Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
            McpHandlerUtils::AddVerification(Resp, ControlRigBlueprint);
          }
        }
      }
    }
    return false;
}
} // namespace McpAnimationHandlers

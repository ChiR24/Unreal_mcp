#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpHandlerUtils.h"
#include "McpVersionCompatibility.h"

#include "Animation/Skeleton.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationSetupIKAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString IKName;
    if (!Payload->TryGetStringField(TEXT("name"), IKName) || IKName.IsEmpty()) {
      Message = TEXT("name field required for IK setup");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) ||
          SkeletonPath.IsEmpty()) {
        Message = TEXT("skeletonPath is required to bind IK to a skeleton");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        USkeleton *TargetSkeleton =
            LoadObject<USkeleton>(nullptr, *SkeletonPath);
        if (!TargetSkeleton) {
          Message = TEXT("Failed to load skeleton for IK");
          ErrorCode = TEXT("LOAD_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          FString FactoryError;
          UBlueprint *ControlRigBlueprint = nullptr;
#if MCP_HAS_CONTROLRIG_FACTORY
          ControlRigBlueprint = Context.Bridge.CreateControlRigBlueprint(
              IKName, SavePath, TargetSkeleton, FactoryError);
#else
          FactoryError =
              TEXT("Control Rig factory not available in this editor build");
#endif
          if (!ControlRigBlueprint) {
            Message = FactoryError.IsEmpty() ? TEXT("Failed to create IK asset")
                                             : FactoryError;
            ErrorCode = TEXT("ASSET_CREATION_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            bSuccess = true;
            Message = TEXT("IK setup created successfully");
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
#endif
} // namespace McpAnimationHandlers

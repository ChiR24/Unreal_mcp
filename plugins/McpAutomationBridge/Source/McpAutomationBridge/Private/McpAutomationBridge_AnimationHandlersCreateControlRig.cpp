#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpVersionCompatibility.h"

#include "Animation/Skeleton.h"
#include "Engine/Blueprint.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateControlRigAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Create a new Control Rig blueprint
    FString RigName;
    if (!Payload->TryGetStringField(TEXT("name"), RigName) || RigName.IsEmpty()) {
      Message = TEXT("name required for create_control_rig");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Rigs");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath required for create_control_rig");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
#if MCP_HAS_CONTROLRIG_FACTORY
        FString FactoryError;
        UBlueprint *ControlRigBP = Context.Bridge.CreateControlRigBlueprint(RigName, SavePath, TargetSkeleton, FactoryError);
        if (ControlRigBP) {
          bSuccess = true;
          Message = TEXT("Control Rig created successfully");
          Resp->SetStringField(TEXT("assetPath"), ControlRigBP->GetPathName());
          Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
        } else {
          Message = FactoryError.IsEmpty() ? TEXT("Failed to create Control Rig") : FactoryError;
          ErrorCode = TEXT("ASSET_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
#else
        Message = TEXT("Control Rig factory not available in this engine version");
        ErrorCode = TEXT("NOT_AVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
#endif
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers

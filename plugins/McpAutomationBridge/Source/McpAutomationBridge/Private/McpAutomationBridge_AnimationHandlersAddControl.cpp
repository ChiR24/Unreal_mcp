#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddControlAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a control to a Control Rig
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString ControlName;
    Payload->TryGetStringField(TEXT("controlName"), ControlName);

    FString ControlType;
    Payload->TryGetStringField(TEXT("controlType"), ControlType);
    if (ControlType.IsEmpty()) {
      ControlType = TEXT("Transform");
    }

    if (AssetPath.IsEmpty() || ControlName.IsEmpty()) {
      Message = TEXT("assetPath and controlName required for add_control");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      Message = TEXT("Control Rig graph mutation is not supported by this build. create_control_rig creates the asset; add controls in the Control Rig editor.");
      ErrorCode = TEXT("NOT_SUPPORTED");
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetStringField(TEXT("controlName"), ControlName);
      Resp->SetStringField(TEXT("controlType"), ControlType);
      Resp->SetStringField(TEXT("error"), Message);
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers

#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationConnectRigElementsAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Connect elements in a Control Rig
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString SourceElement;
    Payload->TryGetStringField(TEXT("sourceElement"), SourceElement);

    FString TargetElement;
    Payload->TryGetStringField(TEXT("targetElement"), TargetElement);

    FString SourcePin;
    Payload->TryGetStringField(TEXT("sourcePin"), SourcePin);

    FString TargetPin;
    Payload->TryGetStringField(TEXT("targetPin"), TargetPin);

    if (AssetPath.IsEmpty() || SourceElement.IsEmpty() || TargetElement.IsEmpty()) {
      Message = TEXT("assetPath, sourceElement, and targetElement required for connect_rig_elements");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      Message = TEXT("Control Rig graph pin connections are not supported by this build. Use the Control Rig editor for graph wiring.");
      ErrorCode = TEXT("NOT_SUPPORTED");
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetStringField(TEXT("sourceElement"), SourceElement);
      Resp->SetStringField(TEXT("targetElement"), TargetElement);
      if (!SourcePin.IsEmpty()) Resp->SetStringField(TEXT("sourcePin"), SourcePin);
      if (!TargetPin.IsEmpty()) Resp->SetStringField(TEXT("targetPin"), TargetPin);
      Resp->SetStringField(TEXT("error"), Message);
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers

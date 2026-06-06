#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddRigUnitAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a rig unit to a Control Rig
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString UnitType;
    Payload->TryGetStringField(TEXT("unitType"), UnitType);

    FString UnitName;
    Payload->TryGetStringField(TEXT("unitName"), UnitName);

    if (AssetPath.IsEmpty() || UnitType.IsEmpty()) {
      Message = TEXT("assetPath and unitType required for add_rig_unit");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      Message = TEXT("Control Rig VM graph unit insertion is not supported by this build. Use the Control Rig editor for rig unit graph edits.");
      ErrorCode = TEXT("NOT_SUPPORTED");
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetStringField(TEXT("unitType"), UnitType);
      Resp->SetStringField(TEXT("unitName"), UnitName);
      Resp->SetStringField(TEXT("error"), Message);
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers

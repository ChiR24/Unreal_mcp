#include "McpAutomationBridge_AnimationHandlersActionContext.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddIKChainAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add an IK chain to an IK Rig
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString ChainName;
    Payload->TryGetStringField(TEXT("chainName"), ChainName);

    FString RootBone;
    Payload->TryGetStringField(TEXT("rootBone"), RootBone);
    if (RootBone.IsEmpty()) {
      Payload->TryGetStringField(TEXT("startBone"), RootBone);
    }

    FString EndBone;
    Payload->TryGetStringField(TEXT("endBone"), EndBone);

    if (AssetPath.IsEmpty() || ChainName.IsEmpty() || RootBone.IsEmpty() || EndBone.IsEmpty()) {
      Message = TEXT("assetPath, chainName, startBone/rootBone, and endBone required for add_ik_chain");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      Message = TEXT("IK Rig chain editing is not supported by this build. Create IK Rig assets through create_ik_rig, then author chains in the IK Rig editor.");
      ErrorCode = TEXT("NOT_SUPPORTED");
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetStringField(TEXT("chainName"), ChainName);
      Resp->SetStringField(TEXT("rootBone"), RootBone);
      Resp->SetStringField(TEXT("endBone"), EndBone);
      Resp->SetStringField(TEXT("error"), Message);
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers

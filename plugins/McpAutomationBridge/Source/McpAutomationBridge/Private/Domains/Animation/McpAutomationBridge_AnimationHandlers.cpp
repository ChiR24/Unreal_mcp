#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionDeclarations.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

bool UMcpAutomationBridgeSubsystem::HandleAnimationPhysicsAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT(">>> HandleAnimationPhysicsAction ENTRY: RequestId=%s RawAction='%s'"),
         *RequestId, *Action);
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("animation_physics"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("animation_physics"))) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("animation_physics payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty()) {
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }
  const FString LowerSub = SubAction.ToLower();
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleAnimationPhysicsAction: subaction='%s'"), *LowerSub);

  // These send their own replies.
  if (LowerSub == TEXT("play_montage") || LowerSub == TEXT("play_anim_montage")) {
    return HandlePlayAnimMontage(RequestId, LowerSub, Payload, RequestingSocket);
  }
  if (LowerSub == TEXT("setup_ragdoll") || LowerSub == TEXT("activate_ragdoll")) {
    return HandleSetupRagdoll(RequestId, LowerSub, Payload, RequestingSocket);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

  McpAnimationHandlers::FActionContext Context{
      *this,
      RequestId,
      RequestingSocket,
      Resp,
      bSuccess,
      Message,
      ErrorCode,
      [this](const FString &Target) -> AActor * {
        return FindActorByName(Target);
      },
  };

  struct FAnimationRoute {
    const TCHAR *Name;
    McpAnimationHandlers::FActionHandler Handler;
  };

  static const FAnimationRoute Routes[] = {
      {TEXT("cleanup"), McpAnimationHandlers::HandleAnimationCleanupAction},
      {TEXT("create_blend_tree"), McpAnimationHandlers::HandleAnimationCreateBlendTreeAction},
      {TEXT("create_procedural_anim"), McpAnimationHandlers::HandleAnimationCreateProceduralAnimAction},
      {TEXT("create_state_machine"), McpAnimationHandlers::HandleAnimationCreateStateMachineAction},
      {TEXT("setup_ik"), McpAnimationHandlers::HandleAnimationSetupIKAction},
      {TEXT("configure_vehicle"), McpAnimationHandlers::HandleAnimationConfigureVehicleAction},
      {TEXT("setup_physics_simulation"), McpAnimationHandlers::HandleAnimationSetupPhysicsSimulationAction},
      {TEXT("create_animation_asset"), McpAnimationHandlers::HandleAnimationCreateAnimationAssetAction},
      {TEXT("create_pose_library"), McpAnimationHandlers::HandleAnimationCreatePoseLibraryAction},
      {TEXT("setup_retargeting"), McpAnimationHandlers::HandleAnimationSetupRetargetingAction},
      {TEXT("skin_mesh_to_skeleton"), McpAnimationHandlers::HandleAnimationSkinMeshToSkeletonAction},
  };

  bool bMatchedAction = false;
  for (const FAnimationRoute &Route : Routes) {
    if (LowerSub == Route.Name) {
      bMatchedAction = true;
      if (Route.Handler(Context, Payload)) {
        return true;
      }
      break;
    }
  }

  if (!bMatchedAction) {
    Context.Fail(TEXT("NOT_IMPLEMENTED"), FString::Printf(
        TEXT("Animation/Physics action '%s' not implemented"), *LowerSub));
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("Animation/Physics action completed")
                       : TEXT("Animation/Physics action failed");
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleAnimationPhysicsAction: responding to subaction '%s' "
              "(success=%s)"),
         *LowerSub, bSuccess ? TEXT("true") : TEXT("false"));
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}

#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridge_AnimationHandlersActionDeclarations.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

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
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleAnimationPhysicsAction: subaction='%s'"), *LowerSub);

#if WITH_EDITOR
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
      [this, &RequestId, RequestingSocket](
          const FString &ForcedAction,
          const TSharedPtr<FJsonObject> &RoutedPayload) -> bool {
        if (ForcedAction == TEXT("play_anim_montage")) {
          return HandlePlayAnimMontage(RequestId, ForcedAction, RoutedPayload,
                                       RequestingSocket);
        }
        if (ForcedAction == TEXT("create_animation_blueprint")) {
          return HandleCreateAnimBlueprint(RequestId, ForcedAction,
                                           RoutedPayload, RequestingSocket);
        }
        return false;
      },
  };

  struct FAnimationRoute {
    const TCHAR *Name;
    McpAnimationHandlers::FActionHandler Handler;
  };

  static const FAnimationRoute Routes[] = {
      {TEXT("cleanup"), McpAnimationHandlers::HandleAnimationCleanupAction},
      {TEXT("create_animation_bp"), McpAnimationHandlers::HandleAnimationCreateAnimationBPAction},
      {TEXT("create_blend_space"), McpAnimationHandlers::HandleAnimationCreateBlendSpaceAction},
      {TEXT("create_blend_tree"), McpAnimationHandlers::HandleAnimationCreateBlendTreeAction},
      {TEXT("create_procedural_anim"), McpAnimationHandlers::HandleAnimationCreateProceduralAnimAction},
      {TEXT("create_state_machine"), McpAnimationHandlers::HandleAnimationCreateStateMachineAction},
      {TEXT("setup_ik"), McpAnimationHandlers::HandleAnimationSetupIKAction},
      {TEXT("configure_vehicle"), McpAnimationHandlers::HandleAnimationConfigureVehicleAction},
      {TEXT("setup_physics_simulation"), McpAnimationHandlers::HandleAnimationSetupPhysicsSimulationAction},
      {TEXT("create_animation_asset"), McpAnimationHandlers::HandleAnimationCreateAnimationAssetAction},
      {TEXT("setup_retargeting"), McpAnimationHandlers::HandleAnimationSetupRetargetingAction},
      {TEXT("play_montage"), McpAnimationHandlers::HandleAnimationPlayMontageAliasAction},
      {TEXT("play_anim_montage"), McpAnimationHandlers::HandleAnimationPlayMontageAliasAction},
      {TEXT("add_notify"), McpAnimationHandlers::HandleAnimationAddNotifyAction},
      {TEXT("add_notify_old_unused"), McpAnimationHandlers::HandleAnimationAddNotifyLegacyAction},
      {TEXT("create_animation_sequence"), McpAnimationHandlers::HandleAnimationCreateAnimationSequenceAction},
      {TEXT("set_sequence_length"), McpAnimationHandlers::HandleAnimationSetSequenceLengthAction},
      {TEXT("add_bone_track"), McpAnimationHandlers::HandleAnimationAddBoneTrackAction},
      {TEXT("set_bone_key"), McpAnimationHandlers::HandleAnimationSetBoneKeyAction},
      {TEXT("set_curve_key"), McpAnimationHandlers::HandleAnimationSetCurveKeyAction},
      {TEXT("create_montage"), McpAnimationHandlers::HandleAnimationCreateMontageAction},
      {TEXT("add_montage_section"), McpAnimationHandlers::HandleAnimationAddMontageSectionAction},
      {TEXT("add_montage_slot"), McpAnimationHandlers::HandleAnimationAddMontageSlotAction},
      {TEXT("set_section_timing"), McpAnimationHandlers::HandleAnimationSetSectionTimingAction},
      {TEXT("add_montage_notify"), McpAnimationHandlers::HandleAnimationAddMontageNotifyAction},
      {TEXT("set_blend_in"), McpAnimationHandlers::HandleAnimationSetBlendInAction},
      {TEXT("set_blend_out"), McpAnimationHandlers::HandleAnimationSetBlendOutAction},
      {TEXT("link_sections"), McpAnimationHandlers::HandleAnimationLinkSectionsAction},
      {TEXT("create_blend_space_1d"), McpAnimationHandlers::HandleAnimationCreateBlendSpace1dAction},
      {TEXT("create_blend_space_2d"), McpAnimationHandlers::HandleAnimationCreateBlendSpace2dAction},
      {TEXT("add_blend_sample"), McpAnimationHandlers::HandleAnimationAddBlendSampleAction},
      {TEXT("set_axis_settings"), McpAnimationHandlers::HandleAnimationSetAxisSettingsAction},
      {TEXT("set_interpolation_settings"), McpAnimationHandlers::HandleAnimationSetInterpolationSettingsAction},
      {TEXT("create_aim_offset"), McpAnimationHandlers::HandleAnimationCreateAimOffsetAction},
      {TEXT("add_aim_offset_sample"), McpAnimationHandlers::HandleAnimationAddAimOffsetSampleAction},
      {TEXT("create_anim_blueprint"), McpAnimationHandlers::HandleAnimationCreateAnimBlueprintAliasAction},
      {TEXT("add_state_machine"), McpAnimationHandlers::HandleAnimationAddStateMachineAction},
      {TEXT("add_state"), McpAnimationHandlers::HandleAnimationAddStateAction},
      {TEXT("add_transition"), McpAnimationHandlers::HandleAnimationAddTransitionAction},
      {TEXT("set_transition_rules"), McpAnimationHandlers::HandleAnimationSetTransitionRulesAction},
      {TEXT("add_blend_node"), McpAnimationHandlers::HandleAnimationAddBlendNodeAction},
      {TEXT("add_cached_pose"), McpAnimationHandlers::HandleAnimationAddCachedPoseAction},
      {TEXT("add_slot_node"), McpAnimationHandlers::HandleAnimationAddSlotNodeAction},
      {TEXT("create_control_rig"), McpAnimationHandlers::HandleAnimationCreateControlRigAction},
      {TEXT("add_control"), McpAnimationHandlers::HandleAnimationAddControlAction},
      {TEXT("add_rig_unit"), McpAnimationHandlers::HandleAnimationAddRigUnitAction},
      {TEXT("connect_rig_elements"), McpAnimationHandlers::HandleAnimationConnectRigElementsAction},
      {TEXT("create_pose_library"), McpAnimationHandlers::HandleAnimationCreatePoseLibraryAction},
      {TEXT("create_ik_rig"), McpAnimationHandlers::HandleAnimationCreateIKRigAction},
      {TEXT("add_ik_chain"), McpAnimationHandlers::HandleAnimationAddIKChainAction},
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
    Message = FString::Printf(
        TEXT("Animation/Physics action '%s' not implemented"), *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
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
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Animation/Physics actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

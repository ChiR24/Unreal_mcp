#include "McpAutomationBridge_ControlActorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("control_actor"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("control_actor")))
    return false;
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("control_actor payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();


#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }
  if (!GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"),
                              TEXT("EditorActorSubsystem not available"), nullptr);
    return true;
  }

  if (LowerSub == TEXT("spawn") || LowerSub == TEXT("spawn_actor"))
    return HandleControlActorSpawn(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("spawn_blueprint"))
    return HandleControlActorSpawnBlueprint(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("delete") || LowerSub == TEXT("remove") ||
      LowerSub == TEXT("destroy_actor"))
    return HandleControlActorDelete(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("apply_force") ||
      LowerSub == TEXT("apply_force_to_actor"))
    return HandleControlActorApplyForce(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_transform") ||
      LowerSub == TEXT("set_actor_transform") ||
      LowerSub == TEXT("teleport_actor") ||
      LowerSub == TEXT("set_actor_location") ||
      LowerSub == TEXT("set_actor_rotation") ||
      LowerSub == TEXT("set_actor_scale"))
    return HandleControlActorSetTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_transform") ||
      LowerSub == TEXT("get_actor_transform"))
    return HandleControlActorGetTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_visibility") ||
      LowerSub == TEXT("set_actor_visible") ||
      LowerSub == TEXT("set_actor_visibility"))
    return HandleControlActorSetVisibility(RequestId, Payload,
                                           RequestingSocket);
  if (LowerSub == TEXT("add_component"))
    return HandleControlActorAddComponent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_component_properties") ||
      LowerSub == TEXT("set_component_property"))
    return HandleControlActorSetComponentProperties(RequestId, Payload,
                                                    RequestingSocket);
  if (LowerSub == TEXT("set_material") ||
      LowerSub == TEXT("set_actor_material") ||
      LowerSub == TEXT("apply_material"))
    return HandleControlActorSetMaterial(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_components") ||
      LowerSub == TEXT("get_actor_components"))
    return HandleControlActorGetComponents(RequestId, Payload,
                                           RequestingSocket);
  if (LowerSub == TEXT("duplicate"))
    return HandleControlActorDuplicate(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("attach") || LowerSub == TEXT("attach_actor"))
    return HandleControlActorAttach(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("detach") || LowerSub == TEXT("detach_actor"))
    return HandleControlActorDetach(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_tag"))
    return HandleControlActorFindByTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("add_tag"))
    return HandleControlActorAddTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("remove_tag"))
    return HandleControlActorRemoveTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_name") || LowerSub == TEXT("find_actors_by_name"))
    return HandleControlActorFindByName(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("delete_by_tag"))
    return HandleControlActorDeleteByTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_blueprint_variables"))
    return HandleControlActorSetBlueprintVariables(RequestId, Payload,
                                                   RequestingSocket);
  if (LowerSub == TEXT("create_snapshot"))
    return HandleControlActorCreateSnapshot(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("restore_snapshot"))
    return HandleControlActorRestoreSnapshot(RequestId, Payload,
                                             RequestingSocket);
  if (LowerSub == TEXT("export"))
    return HandleControlActorExport(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_bounding_box") || LowerSub == TEXT("get_actor_bounds"))
    return HandleControlActorGetBoundingBox(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("get_metadata"))
    return HandleControlActorGetMetadata(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("list") || LowerSub == TEXT("list_actors") || LowerSub == TEXT("list_objects"))
    return HandleControlActorList(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get") || LowerSub == TEXT("get_actor") ||
      LowerSub == TEXT("get_actor_by_name"))
    return HandleControlActorGet(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_class") || LowerSub == TEXT("find_actors_by_class"))
    return HandleControlActorFindByClass(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("remove_component"))
    return HandleControlActorRemoveComponent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_component_property"))
    return HandleControlActorGetComponentProperty(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_property"))
    return HandleSetObjectProperty(RequestId, TEXT("set_object_property"), Payload, RequestingSocket);
  if (LowerSub == TEXT("get_property"))
    return HandleGetObjectProperty(RequestId, TEXT("get_object_property"), Payload, RequestingSocket);
  if (LowerSub == TEXT("set_collision") || LowerSub == TEXT("set_actor_collision"))
    return HandleControlActorSetCollision(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("call_function") || LowerSub == TEXT("call_actor_function"))
    return HandleControlActorCallFunction(RequestId, Payload, RequestingSocket);

  SendStandardErrorResponse(
      this, RequestingSocket, RequestId, TEXT("UNKNOWN_ACTION"),
      FString::Printf(TEXT("Unknown actor control action: %s"), *LowerSub), nullptr);
  return true;
#else
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Actor control requires editor build."), nullptr);
  return true;
#endif
}

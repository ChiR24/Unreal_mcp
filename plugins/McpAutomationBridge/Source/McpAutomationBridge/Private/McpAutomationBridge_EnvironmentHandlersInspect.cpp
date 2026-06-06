#include "McpAutomationBridge_EnvironmentHandlersShared.h"

using namespace McpEnvironmentHandlers;

bool UMcpAutomationBridgeSubsystem::HandleInspectAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("inspect"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("inspect payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction;
    Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSubAction = SubAction.ToLower();

    const bool bIsGlobalAction =
        LowerSubAction.Equals(TEXT("get_project_settings")) ||
        LowerSubAction.Equals(TEXT("get_editor_settings")) ||
        LowerSubAction.Equals(TEXT("get_world_settings")) ||
        LowerSubAction.Equals(TEXT("get_viewport_info")) ||
        LowerSubAction.Equals(TEXT("get_selected_actors")) ||
        LowerSubAction.Equals(TEXT("get_scene_stats")) ||
        LowerSubAction.Equals(TEXT("get_performance_stats")) ||
        LowerSubAction.Equals(TEXT("get_memory_stats")) ||
        LowerSubAction.Equals(TEXT("list_objects")) ||
        LowerSubAction.Equals(TEXT("find_by_class")) ||
        LowerSubAction.Equals(TEXT("find_by_tag")) ||
        LowerSubAction.Equals(TEXT("inspect_class")) ||
        LowerSubAction.Equals(TEXT("inspect_cdo")) ||
        LowerSubAction.Equals(TEXT("runtime_report")) ||
        LowerSubAction.Equals(TEXT("pie_report"));

    const bool bIsActorAction =
        LowerSubAction.Equals(TEXT("get_components")) ||
        LowerSubAction.Equals(TEXT("get_component_property")) ||
        LowerSubAction.Equals(TEXT("set_component_property")) ||
        LowerSubAction.Equals(TEXT("get_metadata")) ||
        LowerSubAction.Equals(TEXT("add_tag")) ||
        LowerSubAction.Equals(TEXT("create_snapshot")) ||
        LowerSubAction.Equals(TEXT("restore_snapshot")) ||
        LowerSubAction.Equals(TEXT("export")) ||
        LowerSubAction.Equals(TEXT("delete_object")) ||
        LowerSubAction.Equals(TEXT("get_bounding_box")) ||
        LowerSubAction.Equals(TEXT("set_property")) ||
        LowerSubAction.Equals(TEXT("get_property"));

    if (bIsActorAction)
    {
        FString ActorAlias;
        Payload->TryGetStringField(TEXT("actorName"), ActorAlias);
        ActorAlias.TrimStartAndEndInline();
        if (ActorAlias.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("name"), ActorAlias);
            ActorAlias.TrimStartAndEndInline();
        }
        if (ActorAlias.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("objectPath"), ActorAlias);
            ActorAlias.TrimStartAndEndInline();
        }
        if (!ActorAlias.IsEmpty())
        {
            Payload->SetStringField(TEXT("actorName"), ActorAlias);
        }

        if (LowerSubAction.Equals(TEXT("get_property")) || LowerSubAction.Equals(TEXT("set_property")))
        {
            FString ObjectPath;
            FString BlueprintPath;
            Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
            Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
            if (ObjectPath.IsEmpty() && BlueprintPath.IsEmpty() && !ActorAlias.IsEmpty())
            {
                Payload->SetStringField(TEXT("objectPath"), ActorAlias);
            }
        }
        else if (LowerSubAction.Equals(TEXT("delete_object")))
        {
            Payload->SetStringField(TEXT("action"), TEXT("delete"));
        }

        return HandleControlActorAction(RequestId, TEXT("control_actor"), Payload, RequestingSocket);
    }

    if (LowerSubAction.Equals(TEXT("inspect_cdo")))
    {
        return HandleInspectCdoAction(RequestId, Payload, RequestingSocket);
    }

    if (bIsGlobalAction)
    {
        return McpEnvironmentHandlers::HandleInspectGlobalAction(
            *this, RequestId, SubAction, LowerSubAction, Payload, RequestingSocket);
    }

    FString ObjectPath;
    Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
    if (ObjectPath.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("actorName"), ObjectPath);
    }
    if (ObjectPath.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("name"), ObjectPath);
    }
    if (ObjectPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("objectPath, actorName, or name required"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    return McpEnvironmentHandlers::HandleInspectObjectAction(
        *this, RequestId, ObjectPath, RequestingSocket);
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("inspect requires editor build"), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "Domains/Blueprint/McpBPActionContext.h"
#include "Foundation/Blueprint/McpBlueprintUtils.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "K2Node_CustomEvent.h"

namespace McpBlueprintGraphHandlers
{
bool TryCreateCustomEventNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    if (NodeType != TEXT("CustomEvent") &&
        NodeType != TEXT("K2Node_CustomEvent"))
    {
        return false;
    }

    // The contract publishes three spellings of this name and add_node honours
    // all three; create_node read only eventName, so a contract-following call
    // that passed customEventName built an event named None. That compiles clean
    // and nothing can ever call it - Set Timer by Function Name and Call Function
    // find no such function - so it surfaces only as logic that never runs.
    FString EventName;
    Context.Payload->TryGetStringField(TEXT("eventName"), EventName);
    if (EventName.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("customEventName"), EventName);
    }
    if (EventName.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("nodeName"), EventName);
    }
    if (EventName.IsEmpty())
    {
        Context.SendError(
            TEXT("A custom event needs a name. Pass eventName (customEventName "
                 "and nodeName are accepted too) - an unnamed custom event "
                 "compiles, but nothing can ever call it."),
            TEXT("MISSING_REQUIRED_PARAMETER"));
        return true;
    }

    // Parameters are typed by the resolver add_event uses, and a type no pin takes is refused before anything changes.
    // This path built a property per parameter on a temporary function, which took no array, set or map, dropped an
    // interface or soft class, and removed an existing event of that name (and a function graph named like it).
    const TArray<TSharedPtr<FJsonValue>>* Parameters = nullptr;
    Context.Payload->TryGetArrayField(TEXT("parameters"), Parameters);
    const TArray<TSharedPtr<FJsonValue>> Params = Parameters ? *Parameters : TArray<TSharedPtr<FJsonValue>>();
    const FString BadParam = McpBlueprintUtils::DescribeInvalidParam(Params, TEXT("parameters"));
    if (!BadParam.IsEmpty())
    {
        Context.SendError(BadParam, TEXT("TYPE_RESOLUTION_FAILED"));
        return true;
    }

    // Two custom events sharing a name is the same compile error that
    // disables a whole graph, already closed for override events.
    TArray<UK2Node_CustomEvent*> Existing;
    FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_CustomEvent>(
        Context.Blueprint,
        Existing);
    for (UK2Node_CustomEvent* Node : Existing)
    {
        if (!Node || Node->CustomFunctionName != FName(*EventName))
        {
            continue;
        }
        TSharedPtr<FJsonObject> Reused = McpHandlerUtils::CreateResultObject();
        const FString ReusedGuid = Node->NodeGuid.ToString();
        Reused->SetStringField(TEXT("nodeGuid"), ReusedGuid);
        Reused->SetStringField(TEXT("nodeId"), ReusedGuid);
        Reused->SetStringField(TEXT("nodeName"), Node->GetName());
        Reused->SetBoolField(TEXT("reusedExistingNode"), true);
        McpGraphLayout::AddNodePlacementFields(Reused, *Node); // where it is, as a reused override event says
        Context.SendResponse(
            FString::Printf(
                TEXT("Custom event '%s' already exists; returned that node."),
                *EventName),
            Reused);
        return true;
    }

    FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(
        *Context.TargetGraph);
    UK2Node_CustomEvent* EventNode =
        NodeCreator.CreateNode(false);
    EventNode->CustomFunctionName = FName(*EventName);
    EventNode->NodePosX = X;
    EventNode->NodePosY = Y;
    NodeCreator.Finalize();
    // An output pin per parameter, after the default pins exist, as the engine's own CreateFromFunction adds them.
    for (const TSharedPtr<FJsonValue>& Param : Params)
    {
        const TSharedPtr<FJsonObject> Object = Param->AsObject();
        McpBlueprintHandlers::FEngineWire_AddUserDefinedPin(EventNode, GetJsonStringField(Object, TEXT("name")),
                                                            GetJsonStringField(Object, TEXT("type")), EGPD_Output);
    }
    Context.FinishCreatedNode(EventNode, X, Y);
    // Regenerates the skeleton class, as add_event does, so a later step of the same batch can call the
    // event: CallFunction resolved nothing until a compile and failed with FUNCTION_NOT_FOUND.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.Blueprint);
    return true;
}
}

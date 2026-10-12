#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintGraphHandlers
{
// Pre-compile health gate: a function graph whose entry node is missing or unnamed
// makes the compiler's conform pass call ReplaceFunctionReferences with NAME_None,
// which is a check() -> fatal editor crash (BlueprintEditorUtils.cpp:5914 in 5.8).
// That state is reachable through pure automation calls (delete_node on a function
// entry, then add_node FunctionEntry), but this also protects against blueprints
// corrupted by any other means. Returns false with the offending graph's name.
static bool ValidateFunctionGraphRoots(UBlueprint* Blueprint, FString& OutBadGraph)
{
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph)
        {
            continue;
        }
        TArray<UK2Node_FunctionEntry*> Entries;
        Graph->GetNodesOfClass<UK2Node_FunctionEntry>(Entries);
        if (Entries.Num() != 1 || !Entries[0] ||
            Entries[0]->FunctionReference.GetMemberName().IsNone())
        {
            OutBadGraph = Graph->GetName();
            return false;
        }
    }
    return true;
}

static void RemoveExistingCustomEvents(
    UBlueprint* Blueprint,
    const FString& EventName)
{
    TArray<UK2Node_CustomEvent*> ExistingEvents;
    FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_CustomEvent>(
        Blueprint,
        ExistingEvents);
    for (UK2Node_CustomEvent* Existing : ExistingEvents)
    {
        if (!Existing ||
            Existing->CustomFunctionName.ToString() != EventName)
        {
            continue;
        }

        const FName FunctionName = Existing->CustomFunctionName;
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph && Graph->GetFName() == FunctionName)
            {
                FBlueprintEditorUtils::RemoveGraph(
                    Blueprint,
                    Graph,
                    EGraphRemoveFlags::Default);
                break;
            }
        }
        FBlueprintEditorUtils::RemoveNode(Blueprint, Existing, true);
    }
}

static bool AddParameters(
    FActionContext& Context,
    UFunction* Function,
    const TArray<TSharedPtr<FJsonValue>>& Parameters)
{
    // A type this path cannot build was dropped from the event with only a log line, and the call answered success.
    TArray<FProperty*> Properties;
    FString Error;
    for (int32 Index = 0; Index < Parameters.Num() && Error.IsEmpty(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Parameter = nullptr;
        FString ParameterName;
        FString ParameterType;
        if (Parameters[Index].IsValid() && Parameters[Index]->TryGetObject(Parameter) && Parameter)
        {
            (*Parameter)->TryGetStringField(TEXT("name"), ParameterName);
            (*Parameter)->TryGetStringField(TEXT("type"), ParameterType);
        }
        FProperty* Property = ParameterName.IsEmpty() ? nullptr
            : CreateCustomEventParameter(Function, ParameterName, ParameterType);
        if (Property)
        {
            Properties.Add(Property);
            continue;
        }
        Error = FString::Printf(
            TEXT("parameters[%d] needs a name and a type a custom event takes, not '%s': float, double, int, int64, "
                 "bool, byte, string, name, text, object, class, softobject, an enum or struct name (Vector, Rotator, "
                 "Transform) or a class name (Actor). For an array, set or map parameter use add_event."),
            Index, *ParameterType);
    }

    // Linked even on a refusal, so the function frees what it made.
    if (Properties.Num() > 0)
    {
        Function->ChildProperties = Properties[0];
        for (int32 Index = 0; Index < Properties.Num() - 1; ++Index)
        {
            Properties[Index]->Next = Properties[Index + 1];
        }
    }
    Function->Bind();
    if (!Error.IsEmpty())
    {
        Context.SendError(Error, TEXT("TYPE_RESOLUTION_FAILED"));
    }
    return Error.IsEmpty();
}

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

    const TArray<TSharedPtr<FJsonValue>>* Parameters = nullptr;
    const bool bHasParameters =
        Context.Payload->TryGetArrayField(
            TEXT("parameters"),
            Parameters) &&
        Parameters->Num() > 0;
    if (!bHasParameters)
    {
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
        Context.FinalizeNode(NodeCreator, EventNode, X, Y);
        // Regenerates the skeleton class, as add_event does, so a later step of the same batch can call the
        // event: CallFunction resolved nothing until a compile and failed with FUNCTION_NOT_FOUND.
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.Blueprint);
        return true;
    }

    // This path compiles synchronously below; refuse loudly (instead of crashing
    // the editor) if any function graph is in the orphaned/unnamed-entry state.
    FString BadGraph;
    if (!ValidateFunctionGraphRoots(Context.Blueprint, BadGraph))
    {
        Context.SendError(
            FString::Printf(
                TEXT("Function graph '%s' has no valid named entry node; compiling now "
                     "would crash the editor (engine check in ReplaceFunctionReferences). "
                     "Recreate the function via add_function, then retry."),
                *BadGraph),
            TEXT("BLUEPRINT_INVALID_STATE"));
        return true;
    }

    if (!Context.Blueprint->GeneratedClass)
    {
        Context.SendError(
            TEXT("Blueprint has no GeneratedClass. Compile it first."),
            TEXT("INVALID_STATE"));
        return true;
    }

    const FName TemporaryFunctionName = MakeUniqueObjectName(
        Context.Blueprint->GeneratedClass,
        UFunction::StaticClass(),
        FName(*FString::Printf(
            TEXT("__TempCustomEventFunc_%s"),
            *EventName)));
    UFunction* TemporaryFunction = NewObject<UFunction>(
        Context.Blueprint->GeneratedClass,
        TemporaryFunctionName,
        RF_Public);
    TemporaryFunction->FunctionFlags =
        FUNC_Public | FUNC_BlueprintCallable;

    // The parameters are checked before the old event goes: a refused call removed it all the same.
    if (!AddParameters(Context, TemporaryFunction, *Parameters))
    {
        TemporaryFunction->MarkAsGarbage();
        return true;
    }
    RemoveExistingCustomEvents(Context.Blueprint, EventName);

    UK2Node_CustomEvent* EventNode =
        UK2Node_CustomEvent::CreateFromFunction(
            FVector2D(X, Y),
            Context.TargetGraph,
            EventName,
            TemporaryFunction,
            false);
    if (!EventNode)
    {
        Context.SendError(
            TEXT("Failed to create custom event from function."),
            TEXT("INTERNAL_ERROR"));
        return true;
    }

    EventNode->CreateNewGuid();
    EventNode->PostPlacedNewNode();
    TemporaryFunction->MarkAsGarbage();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
        Context.Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    // `nodeGuid` is the required output field on blueprint.add_event; see the
    // note in McpBPGraphPrivate.h.
    const FString EventNodeGuidText = EventNode->NodeGuid.ToString();
    Result->SetStringField(TEXT("nodeGuid"), EventNodeGuidText);
    Result->SetStringField(TEXT("nodeId"), EventNodeGuidText);
    Result->SetStringField(TEXT("nodeName"), EventNode->GetName());
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(
        TEXT("Custom event with parameters created using engine API."),
        Result);
    return true;
}
}

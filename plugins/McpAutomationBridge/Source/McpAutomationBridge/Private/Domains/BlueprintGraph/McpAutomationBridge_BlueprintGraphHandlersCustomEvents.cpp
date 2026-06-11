#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
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
    TArray<FProperty*> Properties;
    for (const TSharedPtr<FJsonValue>& ParameterValue : Parameters)
    {
        const TSharedPtr<FJsonObject>& Parameter =
            ParameterValue->AsObject();
        if (!Parameter.IsValid())
        {
            continue;
        }

        FString ParameterName;
        FString ParameterType;
        if (!Parameter->TryGetStringField(
                TEXT("name"),
                ParameterName) ||
            !Parameter->TryGetStringField(
                TEXT("type"),
                ParameterType))
        {
            Context.SendError(
                TEXT("Missing 'name' or 'type' in parameter definition."),
                TEXT("INVALID_PARAMETER"));
            return false;
        }

        if (FProperty* Property = CreateCustomEventParameter(
                Function,
                ParameterName,
                ParameterType))
        {
            Properties.Add(Property);
        }
    }

    if (Properties.Num() > 0)
    {
        Function->ChildProperties = Properties[0];
        for (int32 Index = 0; Index < Properties.Num() - 1; ++Index)
        {
            Properties[Index]->Next = Properties[Index + 1];
        }
    }
    Function->Bind();
    return true;
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

    FString EventName;
    Context.Payload->TryGetStringField(TEXT("eventName"), EventName);
    const TArray<TSharedPtr<FJsonValue>>* Parameters = nullptr;
    const bool bHasParameters =
        Context.Payload->TryGetArrayField(
            TEXT("parameters"),
            Parameters) &&
        Parameters->Num() > 0;
    if (!bHasParameters)
    {
        FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(
            *Context.TargetGraph);
        UK2Node_CustomEvent* EventNode =
            NodeCreator.CreateNode(false);
        EventNode->CustomFunctionName = FName(*EventName);
        Context.FinalizeNode(NodeCreator, EventNode, X, Y);
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

    RemoveExistingCustomEvents(Context.Blueprint, EventName);
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

    if (!AddParameters(Context, TemporaryFunction, *Parameters))
    {
        return true;
    }

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
    Result->SetStringField(
        TEXT("nodeId"),
        EventNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), EventNode->GetName());
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(
        TEXT("Custom event with parameters created using engine API."),
        Result);
    return true;
}
}
#endif

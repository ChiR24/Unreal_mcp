#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_Event.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetTextLibrary.h"

namespace McpBlueprintGraphHandlers
{
static bool TryCreateFunctionNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    // K2Node_PromotableOperator IS a UK2Node_CallFunction subclass whose pins
    // come from its bound math function. It previously fell through to the
    // generic path, which spawned it with no bound function: the node rendered
    // titled "None" and reported success while being unusable. Route it here
    // and bind the reflected operator function instead.
    const bool bPromotable = NodeType == TEXT("PromotableOperator") ||
                             NodeType == TEXT("K2Node_PromotableOperator");
    if (NodeType != TEXT("CallFunction") &&
        NodeType != TEXT("K2Node_CallFunction") &&
        NodeType != TEXT("FunctionCall") &&
        !bPromotable)
    {
        return false;
    }

    FString MemberName;
    FString MemberClass;
    Context.Payload->TryGetStringField(TEXT("memberName"), MemberName);
    Context.Payload->TryGetStringField(TEXT("memberClass"), MemberClass);
    // `targetClass` is published alongside `memberClass` and reads as the obvious way to say
    // which class owns the function, but it was never consulted here. A call naming it was
    // silently resolved by global search instead, so e.g. GetForwardVector landed on the
    // KismetMathLibrary(Rotator) overload rather than the component's - a wrong node that
    // still reported success. Treat it as an alias.
    if (MemberClass.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("targetClass"), MemberClass);
    }
    UClass* ResolvedMemberClass = nullptr;
    UFunction* Function = ResolveGraphCallFunction(Context.Blueprint, MemberName, MemberClass,
                                                   ResolvedMemberClass);

    if (!Function)
    {
        UClass* HintClass = ResolvedMemberClass
                                ? ResolvedMemberClass
                                : Context.Blueprint->GeneratedClass.Get();
        const FString MemberHint = SuggestMemberFix(HintClass, MemberName);
        Context.SendError(
            bPromotable
                ? FString::Printf(
                      TEXT("Promotable operator '%s' resolved no math function. ")
                      TEXT("Use the reflected Kismet function name via memberName ")
                      TEXT("(e.g. Multiply_VectorFloat, Multiply_FloatFloat, ")
                      TEXT("Add_VectorVector, Subtract_FloatFloat, Greater_DoubleDouble); ")
                      TEXT("bare words like 'multiply' resolve nothing."),
                      *MemberName)
                : FString::Printf(
                      TEXT("Function '%s' not found.%s"), *MemberName,
                      *MemberHint),
            TEXT("FUNCTION_NOT_FOUND"));
        return true;
    }

    if (bPromotable)
    {
        FGraphNodeCreator<UK2Node_PromotableOperator> OperatorCreator(
            *Context.TargetGraph);
        UK2Node_PromotableOperator* Operator =
            OperatorCreator.CreateNode(false);
        Operator->SetFromFunction(Function);
        Context.FinalizeNode(OperatorCreator, Operator, X, Y);
        return true;
    }

    // An array-library function (Array_Length, Array_Get, Array_Add, ...) takes a
    // WILDCARD array pin, and only UK2Node_CallArrayFunction propagates the real
    // element type into it when something is connected. Built as a plain
    // CallFunction the pin stayed wildcard forever, so the node connected
    // happily and the blueprint then failed to compile with "The type of Target
    // Array is undetermined" - a message that never reached the caller. The
    // editor picks the node class off this same metadata key.
    if (Function->HasMetaData(TEXT("ArrayParm")))
    {
        FGraphNodeCreator<UK2Node_CallArrayFunction> ArrayCreator(
            *Context.TargetGraph);
        UK2Node_CallArrayFunction* ArrayNode = ArrayCreator.CreateNode(false);
        ArrayNode->SetFromFunction(Function);
        Context.FinalizeNode(ArrayCreator, ArrayNode, X, Y);
        return true;
    }

    FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(
        *Context.TargetGraph);
    UK2Node_CallFunction* Node = NodeCreator.CreateNode(false);
    Node->SetFromFunction(Function);
    Context.FinalizeNode(NodeCreator, Node, X, Y);
    return true;
}

static bool TryCreateEventNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    if (NodeType != TEXT("Event") &&
        NodeType != TEXT("K2Node_Event"))
    {
        return false;
    }

    FString EventName;
    FString MemberClass;
    Context.Payload->TryGetStringField(TEXT("eventName"), EventName);
    // Every other create_node type names its member with memberName; a caller
    // that did the same here got "eventName required".
    if (EventName.IsEmpty()) Context.Payload->TryGetStringField(TEXT("memberName"), EventName);
    Context.Payload->TryGetStringField(TEXT("memberClass"), MemberClass);
    if (EventName.IsEmpty())
    {
        Context.SendError(
            TEXT("eventName (or memberName) required: the event to implement, e.g. BeginPlay or ActorBeginOverlap"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // AActor's overridable events are all Receive<Name> (BeginPlay, Tick,
    // ActorBeginOverlap, Hit, AnyDamage...), and the editor shows them without the
    // prefix, so "ActorBeginOverlap" used to be EVENT_NOT_FOUND. Rewriting
    // unconditionally broke non-Actor graphs that share a name (UUserWidget
    // declares its own `Tick`), so try the name as given first.
    EventName.RemoveFromStart(TEXT("Event "));
    TArray<FString> Candidates;
    Candidates.Add(EventName);
    if (!EventName.StartsWith(TEXT("Receive")))
    {
        Candidates.Add(TEXT("Receive") + EventName);
    }

    UClass* TargetClass = nullptr;
    UFunction* EventFunction = nullptr;
    for (const FString& Candidate : Candidates)
    {
        if (!MemberClass.IsEmpty())
        {
            TargetClass = ResolveUClass(MemberClass);
            if (TargetClass)
            {
                EventFunction = TargetClass->FindFunctionByName(*Candidate);
            }
        }
        else
        {
            for (UClass* Class = Context.Blueprint->ParentClass;
                 Class && !EventFunction;
                 Class = Class->GetSuperClass())
            {
                EventFunction = Class->FindFunctionByName(
                    *Candidate,
                    EIncludeSuperFlag::ExcludeSuper);
                if (EventFunction)
                {
                    TargetClass = Class;
                }
            }
        }
        if (EventFunction)
        {
            EventName = Candidate;
            break;
        }
    }

    if (!EventFunction || !TargetClass)
    {
        // Name what can be overridden instead of leaving the caller to guess spellings.
        TArray<FString> Known;
        UClass* Scan = TargetClass ? TargetClass : Context.Blueprint->ParentClass.Get();
        for (TFieldIterator<UFunction> It(Scan); It && Known.Num() < 30; ++It)
        {
            FString Name = It->GetName();
            if (It->HasAnyFunctionFlags(FUNC_BlueprintEvent) && !It->GetReturnProperty()
                && Name != TEXT("UserConstructionScript"))
            {
                // Shown as the editor shows it; both spellings resolve.
                Name.RemoveFromStart(TEXT("Receive"));
                Known.AddUnique(Name);
            }
        }
        Context.SendError(
            FString::Printf(TEXT("Event '%s' not found on %s. Its overridable events: %s."),
                            *EventName, Scan ? *Scan->GetName() : TEXT("?"),
                            *FString::Join(Known, TEXT(", "))),
            TEXT("EVENT_NOT_FOUND"));
        return true;
    }

    // An event can be implemented only once per Blueprint. Adding a second node
    // for the same function compiles to "Found more than one function with the
    // same name", which disables the WHOLE graph - so a caller that wired new
    // logic onto its own fresh BeginPlay silently got a Blueprint that no longer
    // ran anything. Asking for an event that already exists means that event, so
    // hand the existing node back instead of creating a duplicate.
    for (UEdGraphNode* ExistingNode : Context.TargetGraph->Nodes)
    {
        UK2Node_Event* ExistingEvent = Cast<UK2Node_Event>(ExistingNode);
        if (!ExistingEvent || !ExistingEvent->bOverrideFunction)
        {
            continue;
        }
        if (ExistingEvent->EventReference.GetMemberName() != EventFunction->GetFName())
        {
            continue;
        }
        TSharedPtr<FJsonObject> Existing = McpHandlerUtils::CreateResultObject();
        const FString ExistingGuid = ExistingEvent->NodeGuid.ToString();
        Existing->SetStringField(TEXT("nodeGuid"), ExistingGuid);
        Existing->SetStringField(TEXT("nodeId"), ExistingGuid);
        Existing->SetStringField(TEXT("nodeName"), ExistingEvent->GetName());
        Existing->SetBoolField(TEXT("reusedExistingNode"), true);
        McpGraphLayout::AddNodePlacementFields(Existing, *ExistingEvent);
        McpHandlerUtils::AddVerification(Existing, Context.Blueprint);
        Context.SendResponse(
            FString::Printf(
                TEXT("Event '%s' is already implemented in this graph; returned the existing node."),
                *EventName),
            Existing);
        return true;
    }

    FGraphNodeCreator<UK2Node_Event> NodeCreator(*Context.TargetGraph);
    UK2Node_Event* Node = NodeCreator.CreateNode(false);
    Node->EventReference.SetFromField<UFunction>(EventFunction, false);
    Node->bOverrideFunction = true;
    Context.FinalizeNode(NodeCreator, Node, X, Y);
    return true;
}

bool TryCreateFunctionOrEventNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    return TryCreateFunctionNode(Context, NodeType, X, Y) ||
           TryCreateEventNode(Context, NodeType, X, Y);
}
}
#endif

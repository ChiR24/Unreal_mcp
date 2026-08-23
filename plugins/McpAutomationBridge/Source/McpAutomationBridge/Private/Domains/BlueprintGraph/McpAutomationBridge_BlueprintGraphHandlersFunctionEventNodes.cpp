#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

namespace McpBlueprintGraphHandlers
{
static bool TryCreateFunctionNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    if (NodeType != TEXT("CallFunction") &&
        NodeType != TEXT("K2Node_CallFunction") &&
        NodeType != TEXT("FunctionCall"))
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
    UFunction* Function = nullptr;
    if (!MemberClass.IsEmpty())
    {
        if (UClass* Class = ResolveUClass(MemberClass))
        {
            Function = Class->FindFunctionByName(*MemberName);
        }
    }
    else
    {
        Function =
            Context.Blueprint->GeneratedClass->FindFunctionByName(*MemberName);
        if (!Function)
        {
            Function = UKismetSystemLibrary::StaticClass()
                ->FindFunctionByName(*MemberName);
        }
        if (!Function)
        {
            Function = UGameplayStatics::StaticClass()
                ->FindFunctionByName(*MemberName);
        }
        if (!Function)
        {
            Function = UKismetMathLibrary::StaticClass()
                ->FindFunctionByName(*MemberName);
        }
    }

    if (!Function)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Function '%s' not found"),
                *MemberName),
            TEXT("FUNCTION_NOT_FOUND"));
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
    Context.Payload->TryGetStringField(TEXT("memberClass"), MemberClass);
    if (EventName.IsEmpty())
    {
        Context.SendError(
            TEXT("eventName required"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // These aliases are AActor spellings. Rewriting unconditionally broke every
    // non-Actor graph that happens to share a name: UUserWidget declares its own
    // `Tick` UFUNCTION, so asking for Tick in a widget graph was rewritten to
    // ReceiveTick and rejected as EVENT_NOT_FOUND. Try the name as given first
    // and only fall back to the Actor spelling.
    static const TMap<FString, FString> Aliases = {
        {TEXT("BeginPlay"), TEXT("ReceiveBeginPlay")},
        {TEXT("Tick"), TEXT("ReceiveTick")},
        {TEXT("EndPlay"), TEXT("ReceiveEndPlay")}};
    TArray<FString> Candidates;
    Candidates.Add(EventName);
    if (const FString* Alias = Aliases.Find(EventName))
    {
        Candidates.Add(*Alias);
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
        Context.SendError(
            FString::Printf(TEXT("Event '%s' not found"), *EventName),
            TEXT("EVENT_NOT_FOUND"));
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

#include "Domains/Log/McpScriptErrorCause.h"

#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/EngineVersionComparison.h"
#include "UObject/Script.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
#include "Blueprint/BlueprintExceptionInfo.h"  // in UObject/Script.h before 5.4
#endif

namespace McpScriptErrorCause
{
namespace
{
TSharedPtr<FJsonObject> MakeCause(const TCHAR* Kind, const FName Property, const FString& Detail,
                                  const UEdGraphNode* Node = nullptr)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("kind"), Kind);
    Out->SetStringField(TEXT("property"), Property.ToString());
    Out->SetStringField(TEXT("detail"), Detail);
    if (Node)
    {
        Out->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
    }
    return Out;
}

// The node that hands one Set of the variable its value, if its value pin is wired. That pin is the input named after
// the variable: GetValuePin asserts the pin is an output (a Get node's), so on a Set node it crashed the editor.
const UEdGraphNode* ValueSource(const UK2Node_VariableSet* Set)
{
    const UEdGraphPin* Value = Set->FindPin(Set->GetVarName(), EGPD_Input);
    return Value && Value->LinkedTo.Num() > 0 && Value->LinkedTo[0] ? Value->LinkedTo[0]->GetOwningNode() : nullptr;
}

// Assigned somewhere: a cast or a call can hand back None, so a Set fed by one is the likeliest cause.
TSharedPtr<FJsonObject> FromSets(const TArray<UK2Node_VariableSet*>& Sets, const FName Name, const FString& Blueprint)
{
    for (const UK2Node_VariableSet* Set : Sets)
    {
        const UEdGraphNode* Source = ValueSource(Set);
        if (const UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Source))
        {
            return MakeCause(TEXT("cast_failed"), Name, FString::Printf(
                TEXT("%s is assigned from a Cast To %s that failed: the object it was given is not a %s, so %s stayed None. Use the cast's Cast Failed pin, or check what reaches it."),
                *Name.ToString(), *GetNameSafe(CastNode->TargetType), *GetNameSafe(CastNode->TargetType), *Name.ToString()), CastNode);
        }
        if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Source))
        {
            const FString Function = Call->GetTargetFunction() ? Call->GetTargetFunction()->GetName() : Call->GetName();
            return MakeCause(TEXT("function_returned_none"), Name, FString::Printf(
                TEXT("%s is assigned from %s, which returned None (nothing found, or not ready yet). Check its result with IsValid before using %s."),
                *Name.ToString(), *Function, *Name.ToString()), Call);
        }
    }
    return MakeCause(TEXT("set_but_none"), Name, FString::Printf(
        TEXT("%s is assigned in %s, but was None when read: the Set ran after this read, or was given None."),
        *Name.ToString(), *Blueprint), Sets[0]);
}
} // namespace

TSharedPtr<FJsonObject> Diagnose(const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info)
{
    const FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(StackFrame.MostRecentProperty);
    if (Info.GetType() != EBlueprintExceptionType::AccessViolation || !ActiveObject || !Property)
    {
        return nullptr;
    }
    const FName Name = Property->GetFName();
    // A member of the object that raised it: a variable its Blueprint declares, owned by the class itself (a local
    // variable is owned by its function). MostRecentPropertyContainer would say so directly, but it is 5.1+.
    const UClass* Owner = Property->GetOwner<UClass>();
    const UBlueprint* Blueprint = Owner ? UBlueprint::GetBlueprintFromClass(Owner) : nullptr;
    if (!Blueprint || !ActiveObject->GetClass()->IsChildOf(Owner))
    {
        return MakeCause(TEXT("other"), Name, FString::Printf(
            TEXT("%s is a local variable, a parameter or another object's property, and was None when read."), *Name.ToString()));
    }
    const FString BlueprintName = Blueprint->GetName();
    if (const UObject* Held = Property->GetObjectPropertyValue_InContainer(ActiveObject); Held && !IsValid(Held))
    {
        return MakeCause(TEXT("destroyed"), Name, FString::Printf(
            TEXT("%s still points at %s, which was destroyed. Check it with IsValid before use, or clear %s when it goes."),
            *Name.ToString(), *Held->GetName(), *Name.ToString()));
    }

    // Every Blueprint from the object's own class up to the one declaring the variable may assign it.
    TArray<UK2Node_VariableSet*> Sets;
    for (const UClass* Class = ActiveObject->GetClass(); Class; Class = Class->GetSuperClass())
    {
        if (const UBlueprint* ClassBlueprint = UBlueprint::GetBlueprintFromClass(Class))
        {
            TArray<UK2Node_VariableSet*> Found;
            FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_VariableSet>(ClassBlueprint, Found);
            Sets.Append(Found);
        }
        if (Class == Owner)
        {
            break;
        }
    }
    Sets.RemoveAll([Name](const UK2Node_VariableSet* Set) { return Set->GetVarName() != Name; });
    if (Sets.Num() > 0)
    {
        return FromSets(Sets, Name, BlueprintName);
    }
    const bool bPerInstance = Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance);
    const AActor* Actor = Cast<AActor>(ActiveObject);
    if (!bPerInstance || !Actor)
    {
        return MakeCause(TEXT("never_set"), Name, FString::Printf(
            TEXT("Nothing in %s assigns %s and its default is None. Set it (in BeginPlay, say), or make it Instance Editable and set it on each placed actor."),
            *BlueprintName, *Name.ToString()));
    }
    TSharedPtr<FJsonObject> Cause = MakeCause(TEXT("instance_editable_unset"), Name, FString::Printf(
        TEXT("%s is Instance Editable, nothing in %s assigns it, and %s was left None: set it on the placed actor (or where it is spawned)."),
        *Name.ToString(), *BlueprintName, *Actor->GetActorNameOrLabel()));
    // The placed actor in the edited level carries the same label; value is the caller's choice.
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("objectPath"), Actor->GetActorNameOrLabel());
    Params->SetStringField(TEXT("propertyName"), Name.ToString());
    TSharedPtr<FJsonObject> NextCall = MakeShared<FJsonObject>();
    NextCall->SetStringField(TEXT("operation"), TEXT("execute"));
    NextCall->SetStringField(TEXT("tool"), TEXT("inspect"));
    NextCall->SetStringField(TEXT("action"), TEXT("set_property"));
    NextCall->SetObjectField(TEXT("params"), Params);
    Cause->SetObjectField(TEXT("nextCall"), NextCall);
    return Cause;
}
} // namespace McpScriptErrorCause

#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

#include "Components/ActorComponent.h"

namespace McpNetworkingHandlers
{
// Client-side prediction state: a Blueprint variable that replicates only to the owning client's
// autonomous proxy (CPF_Net + COND_AutonomousOnly). An existing variable of the same type is converted.
bool HandleAddNetworkPredictionData(FNetworkingActionContext& Context)
{
    const FString DataType = GetJsonStringField(Context.Payload, TEXT("dataType")).TrimStartAndEnd();
    FString VariableName = GetJsonStringField(Context.Payload, TEXT("variableName")).TrimStartAndEnd();
    const McpBlueprintUtils::FTypeResolutionResult Resolved = McpBlueprintUtils::ResolvePinType(DataType);
    if (DataType.IsEmpty() || !Resolved.bSuccess)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            DataType.IsEmpty()
                ? FString(TEXT("dataType is required: an add_variable type such as Vector, Rotator, Transform, Float, Int, Bool, or Struct: followed by a struct asset path."))
                : FString::Printf(TEXT("dataType '%s' rejected: %s"), *DataType, *Resolved.OutError),
            TEXT("TYPE_RESOLUTION_FAILED"));
        return true;
    }
    if (VariableName.IsEmpty())
    {
        // The default name only exists for a plain type name; a path or container spec is no identifier.
        for (const TCHAR Character : DataType)
        {
            if (!FChar::IsAlnum(Character))
            {
                Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                    FString::Printf(TEXT("Pass variableName: dataType '%s' cannot form the default name PredictionData_<dataType>."), *DataType),
                    TEXT("INVALID_ARGUMENT"));
                return true;
            }
        }
        VariableName = TEXT("PredictionData_") + DataType;
    }

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, GetJsonStringField(Context.Payload, TEXT("blueprintPath")));
    if (!Blueprint)
    {
        return true;
    }
    UClass* ParentClass = Blueprint->ParentClass;
    if (!ParentClass || !(ParentClass->IsChildOf(AActor::StaticClass()) || ParentClass->IsChildOf(UActorComponent::StaticClass())))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("%s is not an Actor or ActorComponent Blueprint, so its variables cannot replicate; nothing was changed."), *Blueprint->GetName()),
            TEXT("NOT_SUPPORTED"));
        return true;
    }

    const FName VarFName(*VariableName);
    const auto FindVariable = [Blueprint, VarFName]()
    {
        return Blueprint->NewVariables.FindByPredicate([VarFName](const FBPVariableDescription& Desc) { return Desc.VarName == VarFName; });
    };
    FBPVariableDescription* Variable = FindVariable();
    const bool bCreated = Variable == nullptr;
    if (Variable && !(Variable->VarType == Resolved.PinType))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("Variable '%s' already exists as %s, not %s; nothing was changed. Pass another variableName."),
                *VariableName, *McpBlueprintUtils::DescribePinType(Variable->VarType), *McpBlueprintUtils::DescribePinType(Resolved.PinType)),
            TEXT("VARIABLE_TYPE_CONFLICT"));
        return true;
    }
    const bool bAlreadySet = Variable && (Variable->PropertyFlags & CPF_Net) != 0 && Variable->ReplicationCondition == COND_AutonomousOnly;
    Blueprint->Modify();
    if (bCreated)
    {
        const bool bAdded = FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarFName, Resolved.PinType);
        Variable = bAdded ? FindVariable() : nullptr;
        if (!Variable)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                FString::Printf(TEXT("Could not add variable '%s': the name is taken by an inherited property, function or component, or is not a valid name."), *VariableName),
                TEXT("ADD_VARIABLE_FAILED"));
            return true;
        }
    }
    Variable->PropertyFlags |= CPF_Net;
    Variable->ReplicationCondition = COND_AutonomousOnly;
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    const bool bCompiled = McpSafeCompileBlueprint(Blueprint);
    const bool bSaved = McpSafeAssetSave(Blueprint);

    // The variable only reaches clients when the owning actor or component replicates; say so, never flip it.
    UObject* Defaults = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    const AActor* ActorDefaults = Cast<AActor>(Defaults);
    const UActorComponent* ComponentDefaults = Cast<UActorComponent>(Defaults);
    const bool bReplicates = ActorDefaults ? ActorDefaults->GetIsReplicated() : (ComponentDefaults && ComponentDefaults->GetIsReplicated());

    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    ResultJson->SetBoolField(TEXT("success"), bSaved);
    ResultJson->SetStringField(TEXT("variableName"), VariableName);
    ResultJson->SetStringField(TEXT("dataType"), McpBlueprintUtils::DescribePinType(Resolved.PinType));
    ResultJson->SetBoolField(TEXT("created"), bCreated);
    ResultJson->SetBoolField(TEXT("updated"), !bCreated && !bAlreadySet);
    ResultJson->SetBoolField(TEXT("actorReplicates"), bReplicates);
    ResultJson->SetBoolField(TEXT("compiled"), bCompiled);
    ResultJson->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    const FString Message = FString::Printf(TEXT("%s '%s' replicates to the owning client only (COND_AutonomousOnly)%s%s%s"),
        bCreated ? TEXT("Added variable") : (bAlreadySet ? TEXT("Variable already set up:") : TEXT("Converted variable")), *VariableName,
        bReplicates ? TEXT("") : TEXT("; the Blueprint does not replicate, so turn on Replicates (set_net_role ROLE_SimulatedProxy) or nothing reaches clients"),
        bCompiled ? TEXT("") : TEXT("; the Blueprint did not compile cleanly, check its errors"),
        bSaved ? TEXT(".") : TEXT("; saving the Blueprint failed, so the change is in memory only."));
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, bSaved, Message, ResultJson, bSaved ? FString() : TEXT("SAVE_FAILED"));
    return true;
}
}

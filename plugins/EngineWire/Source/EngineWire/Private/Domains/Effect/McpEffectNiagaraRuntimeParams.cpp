#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpEffectPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace McpEffectHandlers
{
namespace
{
// The setter family a user parameter's type takes, spelled as parameterType names it; empty for a type none here sets.
FString UserTypeKind(const FNiagaraTypeDefinition& Type)
{
    if (Type == FNiagaraTypeDefinition::GetFloatDef()) return TEXT("Float");
    if (Type == FNiagaraTypeDefinition::GetIntDef()) return TEXT("Int");
    if (Type == FNiagaraTypeDefinition::GetBoolDef()) return TEXT("Bool");
    if (Type == FNiagaraTypeDefinition::GetVec3Def() || Type == FNiagaraTypeDefinition::GetPositionDef()) return TEXT("Vector");
    if (Type == FNiagaraTypeDefinition::GetColorDef()) return TEXT("Color");
    return FString();
}

const TCHAR* ValueForm(const FString& Kind)
{
    if (Kind == TEXT("Bool")) return TEXT("true, false or a number");
    if (Kind == TEXT("Vector")) return TEXT("{x,y,z} or [x,y,z]");
    if (Kind == TEXT("Color")) return TEXT("{r,g,b,a} or [r,g,b,a]");
    return TEXT("a number");
}

// One value through the component's own setters, which keep it as this placement's override, saved with the level.
// The value is Field of Entry: value in the single form, parameterValue in a parameters entry.
bool WriteUserValue(UNiagaraComponent& Component, const FNiagaraVariableWithOffset& Var, const FString& Kind, const TSharedPtr<FJsonObject>& Entry, const TCHAR* Field)
{
    const FName Name(*Var.GetName().ToString().RightChop(5));
    const TSharedPtr<FJsonValue> Value = Entry->TryGetField(Field);
    const bool bNumber = Value.IsValid() && Value->Type == EJson::Number;
    const bool bBoolean = Value.IsValid() && Value->Type == EJson::Boolean;
    const bool bVector = Value.IsValid() && (Value->Type == EJson::Object || Value->Type == EJson::Array);
    if (Kind == TEXT("Float") && bNumber) Component.SetVariableFloat(Name, static_cast<float>(Value->AsNumber()));
    else if (Kind == TEXT("Int") && bNumber) Component.SetVariableInt(Name, static_cast<int32>(Value->AsNumber()));
    else if (Kind == TEXT("Bool") && (bBoolean || bNumber)) Component.SetVariableBool(Name, bBoolean ? Value->AsBool() : Value->AsNumber() != 0.0);
    else if (Kind == TEXT("Color") && bVector) Component.SetVariableLinearColor(Name, ExtractLinearColorField(Entry, Field, FLinearColor::White));
    else if (Kind == TEXT("Vector") && bVector && Var.GetType() == FNiagaraTypeDefinition::GetPositionDef()) Component.SetVariablePosition(Name, ExtractVectorField(Entry, Field, FVector::ZeroVector));
    else if (Kind == TEXT("Vector") && bVector) Component.SetVariableVec3(Name, ExtractVectorField(Entry, Field, FVector::ZeroVector));
    else return false;
    return true;
}

struct FUserWrite
{
    FString Code;
    FString Error;
};

// One entry: the user parameter of that name ("SpawnRate" or "User.SpawnRate") on the component's own system, in
// that parameter's type. A name the system lacks answered success: the component's setter adds any name it is given.
FUserWrite WriteEntry(UNiagaraComponent& Component, const FString& Name, const FString& TypeHint, const TSharedPtr<FJsonObject>& Entry, const TCHAR* Field)
{
    FUserWrite Write;
    UNiagaraSystem* System = Component.GetAsset();
    const FString UserName = Name.StartsWith(TEXT("User.")) ? Name : TEXT("User.") + Name;
    const TArrayView<const FNiagaraVariableWithOffset> Vars =
        System ? System->GetExposedParameters().ReadParameterVariables() : TArrayView<const FNiagaraVariableWithOffset>();
    const FNiagaraVariableWithOffset* Var = Vars.FindByPredicate(
        [&UserName](const FNiagaraVariableWithOffset& Candidate) { return Candidate.GetName().ToString() == UserName; });
    if (!Var)
    {
        TArray<FString> Known;
        for (const FNiagaraVariableWithOffset& Candidate : Vars)
        {
            Known.Add(Candidate.GetName().ToString().RightChop(5));
        }
        Write.Code = TEXT("PARAM_NOT_FOUND");
        Write.Error = FString::Printf(TEXT("'%s' is not a user parameter of %s; it has: %s."), *Name,
            System ? *System->GetName() : TEXT("this component (it plays no system)"), Known.Num() > 0 ? *FString::Join(Known, TEXT(", ")) : TEXT("none"));
        return Write;
    }
    const FString Kind = UserTypeKind(Var->GetType());
    const FString Type = Kind.IsEmpty() ? Var->GetType().GetName() : Kind;
    Write.Code = TEXT("PARAM_TYPE_MISMATCH");
    if (Kind.IsEmpty())
    {
        Write.Error = FString::Printf(TEXT("'%s' is a %s user parameter; this call sets Float, Int, Bool, Vector and Color ones."), *Name, *Type);
    }
    else if (!TypeHint.IsEmpty() && !TypeHint.Equals(Kind, ESearchCase::IgnoreCase) &&
             !(Kind == TEXT("Color") && TypeHint.Equals(TEXT("LinearColor"), ESearchCase::IgnoreCase)))
    {
        Write.Error = FString::Printf(TEXT("'%s' is a %s user parameter, not %s."), *Name, *Type, *TypeHint);
    }
    else if (!WriteUserValue(Component, *Var, Kind, Entry, Field))
    {
        Write.Error = FString::Printf(TEXT("'%s' is a %s user parameter: %s must be %s."), *Name, *Type, Field, ValueForm(Kind));
    }
    else
    {
        Write.Code.Reset();
    }
    return Write;
}

// The Niagara component the call names, else the actor's first; Names collects every one for a miss.
UNiagaraComponent* FindNiagaraComponent(AActor& Actor, const FString& ComponentName, TArray<FString>& Names)
{
    TInlineComponentArray<UNiagaraComponent*> Components(&Actor);
    for (UNiagaraComponent* Component : Components)
    {
        Names.Add(Component->GetName());
        if (ComponentName.IsEmpty() || Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
        {
            return Component;
        }
    }
    return nullptr;
}
}

// A placed effect's own values: one user parameter (parameterName, value), or several (parameters) in one call, on the
// actor's Niagara component (componentName picks one of several).
bool HandleSetNiagaraParameter(const FEffectActionContext& Context)
{
    const FString ActorName = McpGetFirstStringField(Context.Payload, {TEXT("actorName"), TEXT("systemName")});
    FString ComponentName;
    Context.Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    FString ParameterName;
    Context.Payload->TryGetStringField(TEXT("parameterName"), ParameterName);
    FString ParameterType;
    Context.Payload->TryGetStringField(TEXT("parameterType"), ParameterType);
    const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
    const bool bList = Context.Payload->TryGetArrayField(TEXT("parameters"), List) && List && List->Num() > 0;
    if (bList == !ParameterName.IsEmpty())
    {
        Context.Bridge.SendAutomationResponse(Context.Socket, Context.RequestId, false,
            bList ? TEXT("Send parameters or parameterName with value, not both.") : TEXT("parameterName (with value) or parameters required"),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    AActor* Actor = FindActorByLabel(ActorName);
    TArray<FString> ComponentNames;
    UNiagaraComponent* Component = Actor ? FindNiagaraComponent(*Actor, ComponentName, ComponentNames) : nullptr;
    if (!Component)
    {
        const FString Message = !Actor ? FString::Printf(TEXT("Actor '%s' not found"), *ActorName)
            : ComponentNames.Num() > 0 ? FString::Printf(TEXT("Actor '%s' has no Niagara component '%s'; it has: %s"), *ActorName, *ComponentName, *FString::Join(ComponentNames, TEXT(", ")))
            : FString::Printf(TEXT("Actor '%s' has no Niagara component"), *ActorName);
        Context.Bridge.SendAutomationResponse(Context.Socket, Context.RequestId, false, Message, nullptr,
            Actor ? TEXT("COMPONENT_NOT_FOUND") : TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    // For undo, and to mark the level unsaved: the values were lost at the next start unless another edit had.
    Component->Modify();
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetStringField(TEXT("componentName"), Component->GetName());
    if (!bList)
    {
        const FUserWrite Write = WriteEntry(*Component, ParameterName, ParameterType, Context.Payload, TEXT("value"));
        Response->SetStringField(TEXT("parameterName"), ParameterName);
        Context.Bridge.SendAutomationResponse(Context.Socket, Context.RequestId, Write.Code.IsEmpty(),
            Write.Code.IsEmpty() ? TEXT("Niagara parameter set") : *Write.Error, Response, Write.Code);
        return true;
    }
    TArray<TSharedPtr<FJsonValue>> Results;
    TArray<FString> Failed;
    for (const TSharedPtr<FJsonValue>& Item : *List)
    {
        const TSharedPtr<FJsonObject> Entry = Item.IsValid() && Item->Type == EJson::Object ? Item->AsObject() : MakeShared<FJsonObject>();
        FString Name;
        Entry->TryGetStringField(TEXT("parameterName"), Name);
        const FUserWrite Write = WriteEntry(*Component, Name, FString(), Entry, TEXT("parameterValue"));
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("parameterName"), Name);
        Result->SetBoolField(TEXT("applied"), Write.Code.IsEmpty());
        if (!Write.Code.IsEmpty())
        {
            Result->SetStringField(TEXT("error"), Write.Error);
            Failed.Add(Name);
        }
        Results.Add(MakeShared<FJsonValueObject>(Result));
    }
    Response->SetArrayField(TEXT("parameters"), Results);
    Response->SetNumberField(TEXT("applied"), Results.Num() - Failed.Num());
    const bool bAll = Failed.Num() == 0;
    Context.Bridge.SendAutomationResponse(Context.Socket, Context.RequestId, bAll,
        bAll ? FString::Printf(TEXT("Set %d parameters."), Results.Num())
             : FString::Printf(TEXT("Not applied: %s; the others were written."), *FString::Join(Failed, TEXT(", "))),
        Response, bAll ? FString() : FString(TEXT("PARAMETER_BATCH_INCOMPLETE")));
    return true;
}
}

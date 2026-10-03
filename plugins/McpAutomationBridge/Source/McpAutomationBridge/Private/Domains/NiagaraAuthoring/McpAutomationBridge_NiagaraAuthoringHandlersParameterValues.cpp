#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

namespace McpNiagaraAuthoringHandlers
{
// One parameter-store value (user or rapid-iteration) in the variable's own type. Position types are skipped: the
// store asserts unless it already tracks position data under that name. Payload holds parameterValue.
static bool WriteRapidIterationValue(FNiagaraParameterStore& Store, const FNiagaraVariable& Var, const TSharedPtr<FJsonObject>& Payload)
{
    const FNiagaraTypeDefinition& Type = Var.GetType();
    double Num = 0;
    bool Bool = false;
    const TSharedPtr<FJsonObject>* Obj = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    const bool bNum = Payload->TryGetNumberField(TEXT("parameterValue"), Num);
    const bool bBool = Payload->TryGetBoolField(TEXT("parameterValue"), Bool);
    Payload->TryGetObjectField(TEXT("parameterValue"), Obj);
    Payload->TryGetArrayField(TEXT("parameterValue"), Arr);
    if (Type == FNiagaraTypeDefinition::GetFloatDef() && bNum) return Store.SetParameterValue(static_cast<float>(Num), Var);
    if ((Type == FNiagaraTypeDefinition::GetIntDef() || Type.IsEnum()) && bNum) return Store.SetParameterValue(static_cast<int32>(Num), Var);
    if (Type == FNiagaraTypeDefinition::GetBoolDef() && (bBool || bNum)) return Store.SetParameterValue(FNiagaraBool(bBool ? Bool : Num != 0.0), Var);
    // Vector-like values come as an object or, like every other vector here, as an array.
    if (!Obj && !(Arr && Arr->Num() >= 2)) return false;
    if (Type == FNiagaraTypeDefinition::GetColorDef()) return Store.SetParameterValue(ExtractLinearColorField(Payload, TEXT("parameterValue"), FLinearColor::White), Var);
    FVector V = ExtractVectorField(Payload, TEXT("parameterValue"), FVector::ZeroVector);
    if (!Obj && Arr->Num() == 2) V = FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), 0.0);
    const double W = Obj ? GetJsonNumberField(*Obj, TEXT("w"), 0.0) : (Arr->Num() > 3 ? (*Arr)[3]->AsNumber() : 0.0);
    if (Type == FNiagaraTypeDefinition::GetVec3Def()) return Store.SetParameterValue(FVector3f(V), Var);
    if (Type == FNiagaraTypeDefinition::GetVec2Def()) return Store.SetParameterValue(FVector2f(V.X, V.Y), Var);
    if (Type == FNiagaraTypeDefinition::GetVec4Def()) return Store.SetParameterValue(FVector4f(V.X, V.Y, V.Z, static_cast<float>(W)), Var);
    return false;
}

// A module input ("InitializeParticle.Lifetime" or the full "Constants.<Emitter>.<Module>.<Input>")
// lives in the rapid-iteration store of every script that runs it, and emitter-stage modules are
// mirrored into the system scripts, so every copy is written - what the Niagara stack editor does.
// Returns the number of copies written; fills Candidates with the names that did not match and
// MatchedType with the type of a match whose value could not be converted. The caller requests the compile.
int32 SetModuleInputValue(UNiagaraSystem* System, const FString& EmitterName, const FString& ParamName, const TSharedPtr<FJsonObject>& Payload, TArray<FString>& Candidates, FString& MatchedType)
{
    const TArray<UNiagaraScript*> Scripts = GatherModuleInputScripts(System);
    const FString Scope = EmitterName.IsEmpty() ? FString() : TEXT("Constants.") + EmitterName + TEXT(".");
    int32 Written = 0;
    for (UNiagaraScript* Script : Scripts)
    {
        for (const FNiagaraVariableWithOffset& Entry : Script ? Script->RapidIterationParameters.ReadParameterVariables() : TArrayView<const FNiagaraVariableWithOffset>())
        {
            const FString Name = Entry.GetName().ToString();
            if (!Scope.IsEmpty() && !Name.StartsWith(Scope)) continue;
            if (Name != ParamName && !Name.EndsWith(TEXT(".") + ParamName))
            {
                Candidates.AddUnique(Name);
                continue;
            }
            Script->Modify();
            MatchedType = Entry.GetType().GetName();
            Written += WriteRapidIterationValue(Script->RapidIterationParameters, FNiagaraVariable(Entry), Payload) ? 1 : 0;
        }
    }
    return Written;
}

struct FParameterWrite
{
    bool bApplied = false;
    int32 InputCopies = 0;
    FString ErrorCode;
    FString Error;
};

// One value written: the user parameter of that name, else the module input. It replies and saves nothing, so a
// list writes every entry and saves once. Entry holds parameterValue (the payload itself, or one list entry).
static FParameterWrite WriteParameter(const FActionContext& Context, UNiagaraSystem* System, const FString& ParamName, const TSharedPtr<FJsonObject>& Entry)
{
    static const TCHAR* ValueForms = TEXT("parameterValue must be a number (float, int or bool) or an {x,y,z[,w]} or {r,g,b,a} object (or an array) to match.");
    FParameterWrite Write;
    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    const FString UserName = ParamName.StartsWith(TEXT("User.")) ? ParamName : TEXT("User.") + ParamName;
    const FNiagaraVariableWithOffset* UserVar = UserStore.ReadParameterVariables().FindByPredicate(
        [&UserName](const FNiagaraVariableWithOffset& Var) { return Var.GetName().ToString() == UserName; });
    if (UserVar)
    {
        Write.bApplied = WriteRapidIterationValue(UserStore, FNiagaraVariable(*UserVar), Entry);
        if (!Write.bApplied)
        {
            Write.ErrorCode = TEXT("PARAM_TYPE_MISMATCH");
            Write.Error = FString::Printf(TEXT("User parameter '%s' is a %s; %s"), *ParamName, *UserVar->GetType().GetName(), ValueForms);
        }
        return Write;
    }
    // Not a user parameter: try the module inputs, which is what tuning a template emitter needs.
    TArray<FString> Candidates;
    FString MatchedType;
    Write.InputCopies = SetModuleInputValue(System, Context.EmitterName, ParamName, Entry, Candidates, MatchedType);
    // Nor a module input: a module's static switch ("InitializeParticle.Ribbon Width Mode"), which gates inputs.
    FString SwitchError;
    if (Write.InputCopies == 0 && MatchedType.IsEmpty())
    {
        Write.InputCopies = SetModuleStaticSwitch(System, Context.EmitterName, ParamName, Entry->TryGetField(TEXT("parameterValue")), SwitchError);
    }
    Write.bApplied = Write.InputCopies > 0;
    if (Write.bApplied)
    {
        return Write;
    }
    Write.ErrorCode = MatchedType.IsEmpty() && SwitchError.IsEmpty() ? TEXT("PARAM_NOT_FOUND") : TEXT("PARAM_TYPE_MISMATCH");
    if (!SwitchError.IsEmpty())
    {
        Write.Error = SwitchError;
        return Write;
    }
    if (!MatchedType.IsEmpty())
    {
        Write.Error = FString::Printf(TEXT("Module input '%s' is a %s; %s"), *ParamName, *MatchedType, ValueForms);
        return Write;
    }
    // An error message is capped in transit, so the full list lives in get_niagara_info;
    // name a few here, without the scope prefix every one of them shares.
    const FString Scope = TEXT("Constants.") + Context.EmitterName + TEXT(".");
    for (FString& Candidate : Candidates)
    {
        Candidate.RemoveFromStart(Scope);
    }
    Candidates.SetNum(FMath::Min(Candidates.Num(), 8));
    Write.Error = FString::Printf(TEXT("Parameter '%s' is neither a user parameter nor a module input or static switch%s. get_niagara_info lists every emitter's moduleInputs and staticSwitches with their values; some here: %s"),
        *ParamName, Context.EmitterName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" of emitter '%s'"), *Context.EmitterName),
        *FString::Join(Candidates, TEXT(", ")));
    return Write;
}

// parameters: every entry written, then one compile request for the module inputs and one save, and every entry
// reported as the material parameters list reports its own, so twelve values of an emitter cost one call.
static bool SetParameterValueList(FActionContext& Context, UNiagaraSystem* System, const TArray<TSharedPtr<FJsonValue>>& Entries)
{
    TArray<TSharedPtr<FJsonValue>> Results;
    TArray<FString> Failed;
    bool bWroteModuleInput = false;
    for (const TSharedPtr<FJsonValue>& Item : Entries)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        const TSharedPtr<FJsonObject>* Object = nullptr;
        if (Item.IsValid() && Item->TryGetObject(Object))
        {
            Entry = *Object;
        }
        const FString Name = GetJsonStringField(Entry, TEXT("parameterName"));
        FParameterWrite Write;
        if (Name.IsEmpty())
        {
            Write.Error = TEXT("Missing 'parameterName'.");
        }
        else
        {
            Write = WriteParameter(Context, System, Name, Entry);
        }
        bWroteModuleInput = bWroteModuleInput || Write.InputCopies > 0;
        TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
        Row->SetStringField(TEXT("parameterName"), Name);
        Row->SetBoolField(TEXT("applied"), Write.bApplied);
        if (Write.bApplied)
        {
            if (Write.InputCopies > 0) Row->SetNumberField(TEXT("moduleInputCopiesWritten"), Write.InputCopies);
        }
        else
        {
            Row->SetStringField(TEXT("error"), Write.Error);
            Failed.Add(FString::Printf(TEXT("%s: %s"), *Name, *Write.Error));
        }
        Results.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (bWroteModuleInput)
    {
        System->RequestCompile(false);
    }
    const int32 Applied = Results.Num() - Failed.Num();
    if (Applied > 0)
    {
        MarkDirtyAndVerify(Context, System);
    }
    Context.Result->SetArrayField(TEXT("parameters"), Results);
    Context.Result->SetNumberField(TEXT("applied"), Applied);
    if (Failed.Num() > 0)
    {
        Context.Subsystem->SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false,
            FString::Printf(TEXT("Set %d of %d parameters; %s"), Applied, Results.Num(), *FString::Join(Failed, TEXT("; "))),
            Context.Result, TEXT("PARAMETER_BATCH_INCOMPLETE"));
        return true;
    }
    Context.SendSuccess(true, FString::Printf(TEXT("Set %d parameters."), Applied));
    return true;
}

bool SetParameterValue(FActionContext& Context)
{
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"));
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (Context.Payload->TryGetArrayField(TEXT("parameters"), Entries) && Entries->Num() > 0)
    {
        if (!ParamName.IsEmpty())
        {
            Context.SendError(TEXT("Send parameters (a list of {parameterName, parameterValue}), or parameterName with parameterValue, not both."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        return SetParameterValueList(Context, System, *Entries);
    }
    if (ParamName.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'parameterName' (or a 'parameters' list)."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FParameterWrite Write = WriteParameter(Context, System, ParamName, Context.Payload);
    if (!Write.bApplied)
    {
        Context.SendError(Write.Error, Write.ErrorCode);
        return true;
    }
    if (Write.InputCopies > 0)
    {
        System->RequestCompile(false);
        Context.Result->SetNumberField(TEXT("moduleInputCopiesWritten"), Write.InputCopies);
    }
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("parameterName"), ParamName);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set parameter '%s' value."), *ParamName));
    Context.SendSuccess(true, TEXT("Parameter value set."));
    return true;
}
}

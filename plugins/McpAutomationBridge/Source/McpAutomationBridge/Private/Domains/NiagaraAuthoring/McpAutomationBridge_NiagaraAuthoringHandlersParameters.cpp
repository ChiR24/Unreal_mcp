#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
namespace McpNiagaraAuthoringHandlers
{
static bool AddUserParameter(FActionContext& Context)
{
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"));
    const FString ParamType = GetJsonStringField(Context.Payload, TEXT("parameterType"), TEXT("Float"));
    if (ParamName.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    System->GetExposedParameters().AddParameter(FNiagaraVariable(ResolveNiagaraTypeByName(ParamType), FName(*ParamName)), true);
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("parameterName"), ParamName);
    Context.Result->SetStringField(TEXT("parameterType"), ParamType);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added user parameter '%s' of type %s."), *ParamName, *ParamType));
    Context.SendSuccess(true, TEXT("User parameter added."));
    return true;
}

// One rapid-iteration value in the variable's own type. Position types are skipped: the store asserts
// unless it already tracks position data under that name.
static bool WriteRapidIterationValue(FNiagaraParameterStore& Store, const FNiagaraVariable& Var, const TSharedPtr<FJsonObject>& Payload)
{
    const FNiagaraTypeDefinition& Type = Var.GetType();
    double Num = 0;
    bool Bool = false;
    const TSharedPtr<FJsonObject>* Obj = nullptr;
    const bool bNum = Payload->TryGetNumberField(TEXT("parameterValue"), Num);
    const bool bBool = Payload->TryGetBoolField(TEXT("parameterValue"), Bool);
    Payload->TryGetObjectField(TEXT("parameterValue"), Obj);
    if (Type == FNiagaraTypeDefinition::GetFloatDef() && bNum) return Store.SetParameterValue(static_cast<float>(Num), Var);
    if (Type == FNiagaraTypeDefinition::GetIntDef() && bNum) return Store.SetParameterValue(static_cast<int32>(Num), Var);
    if (Type == FNiagaraTypeDefinition::GetBoolDef() && (bBool || bNum)) return Store.SetParameterValue(FNiagaraBool(bBool ? Bool : Num != 0.0), Var);
    if (!Obj) return false;
    if (Type == FNiagaraTypeDefinition::GetColorDef()) return Store.SetParameterValue(GetColorFromJson(*Obj), Var);
    const FVector V = GetVectorFromJson(*Obj);
    if (Type == FNiagaraTypeDefinition::GetVec3Def()) return Store.SetParameterValue(FVector3f(V), Var);
    if (Type == FNiagaraTypeDefinition::GetVec2Def()) return Store.SetParameterValue(FVector2f(V.X, V.Y), Var);
    if (Type == FNiagaraTypeDefinition::GetVec4Def()) return Store.SetParameterValue(FVector4f(V.X, V.Y, V.Z, static_cast<float>(GetJsonNumberField(*Obj, TEXT("w"), 0.0))), Var);
    return false;
}

// A module input ("InitializeParticle.Lifetime" or the full "Constants.<Emitter>.<Module>.<Input>")
// lives in the rapid-iteration store of every script that runs it, and emitter-stage modules are
// mirrored into the system scripts, so every copy is written - what the Niagara stack editor does.
// Returns the number of copies written; fills Candidates with the names that did not match and
// MatchedType with the type of a match whose value could not be converted.
static int32 SetModuleInputValue(FActionContext& Context, UNiagaraSystem* System, const FString& ParamName, TArray<FString>& Candidates, FString& MatchedType)
{
    TArray<UNiagaraScript*> Scripts{System->GetSystemSpawnScript(), System->GetSystemUpdateScript()};
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        if (const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData())
        {
            Data->GetScripts(Scripts, false);
        }
    }
    const FString Scope = Context.EmitterName.IsEmpty() ? FString() : TEXT("Constants.") + Context.EmitterName + TEXT(".");
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
            Written += WriteRapidIterationValue(Script->RapidIterationParameters, FNiagaraVariable(Entry), Context.Payload) ? 1 : 0;
        }
    }
    if (Written > 0)
    {
        System->RequestCompile(false);
    }
    return Written;
}

static bool SetParameterValue(FActionContext& Context)
{
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"));
    if (ParamName.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    FNiagaraVariable FloatVar(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParamName));
    FNiagaraVariable IntVar(FNiagaraTypeDefinition::GetIntDef(), FName(*ParamName));
    FNiagaraVariable BoolVar(FNiagaraTypeDefinition::GetBoolDef(), FName(*ParamName));
    FNiagaraVariable VecVar(FNiagaraTypeDefinition::GetVec3Def(), FName(*ParamName));
    double NumVal = 0;
    bool BoolVal = false;
    Context.Payload->TryGetNumberField(TEXT("parameterValue"), NumVal);
    Context.Payload->TryGetBoolField(TEXT("parameterValue"), BoolVal);
    if (UserStore.FindParameterVariable(FloatVar))
    {
        UserStore.SetParameterValue(static_cast<float>(NumVal), FloatVar);
    }
    else if (UserStore.FindParameterVariable(IntVar))
    {
        UserStore.SetParameterValue(static_cast<int32>(NumVal), IntVar);
    }
    else if (UserStore.FindParameterVariable(BoolVar))
    {
        UserStore.SetParameterValue(FNiagaraBool(BoolVal), BoolVar);
    }
    else if (UserStore.FindParameterVariable(VecVar))
    {
        const TSharedPtr<FJsonObject>* ValObj;
        if (Context.Payload->TryGetObjectField(TEXT("parameterValue"), ValObj))
        {
            UserStore.SetParameterValue(GetVectorFromJson(*ValObj), VecVar);
        }
    }
    else
    {
        // Not a user parameter: try the module inputs, which is what tuning a template emitter needs.
        TArray<FString> Candidates;
        FString MatchedType;
        const int32 Written = SetModuleInputValue(Context, System, ParamName, Candidates, MatchedType);
        if (Written == 0 && !MatchedType.IsEmpty())
        {
            Context.SendError(FString::Printf(TEXT("Module input '%s' is a %s; parameterValue must be a number (float/int/bool) or an {x,y,z[,w]} / {r,g,b,a} object to match."),
                *ParamName, *MatchedType), TEXT("PARAM_TYPE_MISMATCH"));
            return true;
        }
        if (Written == 0)
        {
            Candidates.SetNum(FMath::Min(Candidates.Num(), 40));
            Context.SendError(FString::Printf(TEXT("Parameter '%s' is neither a user parameter nor a module input%s. Module inputs here: %s"),
                *ParamName, Context.EmitterName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" of emitter '%s'"), *Context.EmitterName),
                *FString::Join(Candidates, TEXT(", "))), TEXT("PARAM_NOT_FOUND"));
            return true;
        }
        Context.Result->SetNumberField(TEXT("moduleInputCopiesWritten"), Written);
    }
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("parameterName"), ParamName);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set parameter '%s' value."), *ParamName));
    Context.SendSuccess(true, TEXT("Parameter value set."));
    return true;
}

static FString NormalizeDefaultSource(const FString& SourceBinding)
{
    if (SourceBinding == TEXT("Emitter.Age")) return TEXT("Emitter Age");
    if (SourceBinding == TEXT("Emitter.NormalizedAge")) return TEXT("Emitter Normalized Age");
    if (SourceBinding == TEXT("System.Age")) return TEXT("System Age");
    return SourceBinding;
}

static bool BindParameterToSource(FActionContext& Context)
{
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"));
    const FString SourceBinding = GetJsonStringField(Context.Payload, TEXT("sourceBinding"));
    if (Context.SystemPath.IsEmpty() || ParamName.IsEmpty() || SourceBinding.IsEmpty())
    {
        Context.SendError(Context.SystemPath.IsEmpty() ? TEXT("Missing 'systemPath'.") : TEXT("Missing 'parameterName' or 'sourceBinding'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!ValidateNiagaraIdentifier(Context, ParamName, TEXT("parameterName"), true) || !ValidateNiagaraIdentifier(Context, SourceBinding, TEXT("sourceBinding"), true))
    {
        return true;
    }
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES
    FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, Context.EmitterName);
    if (!Handle)
    {
        Context.SendError(FString::Printf(TEXT("Emitter '%s' not found."), *Context.EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return true;
    }
    UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(Handle);
    UNiagaraGraph* Graph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
    UNiagaraNodeOutput* TargetOutput = nullptr;
    if (!Graph)
    {
        Context.SendError(TEXT("Emitter has no Niagara graph source."), TEXT("NIAGARA_GRAPH_MISSING"));
        return true;
    }
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node); OutputNode && OutputNode->GetUsage() == ENiagaraScriptUsage::ParticleUpdateScript)
        {
            TargetOutput = OutputNode;
            break;
        }
    }
    if (!TargetOutput)
    {
        Context.SendError(TEXT("Emitter has no particle update stack output for parameter binding."), TEXT("NIAGARA_STACK_MISSING"));
        return true;
    }
    const FString NiagaraDefaultSource = NormalizeDefaultSource(SourceBinding);
    const FNiagaraVariable TargetVariable(ResolveNiagaraTypeByName(GetJsonStringField(Context.Payload, TEXT("parameterType"), TEXT("Float"))), FName(*ParamName));
    TArray<FNiagaraVariable> TargetVariables;
    TargetVariables.Add(TargetVariable);
    TArray<FString> DefaultValues;
    DefaultValues.Add(NiagaraDefaultSource);
    Graph->Modify();
    UNiagaraNodeAssignment* AssignmentNode = FNiagaraStackGraphUtilities::AddParameterModuleToStack(TargetVariables, *TargetOutput, INDEX_NONE, DefaultValues);
    if (!AssignmentNode)
    {
        Context.SendError(TEXT("Failed to create Niagara assignment module for parameter binding."), TEXT("NIAGARA_BINDING_FAILED"));
        return true;
    }
    AssignmentNode->RefreshFromExternalChanges();
    AssignmentNode->UpdateUsageBitmaskFromOwningScript();
    Graph->NotifyGraphChanged();
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetBoolField(TEXT("bindingApplied"), true);
    Context.Result->SetBoolField(TEXT("assignmentModuleAdded"), true);
    Context.Result->SetStringField(TEXT("parameterName"), ParamName);
    Context.Result->SetStringField(TEXT("sourceBinding"), SourceBinding);
    Context.Result->SetStringField(TEXT("niagaraDefaultSource"), NiagaraDefaultSource);
    Context.Result->SetStringField(TEXT("assignmentNodeId"), AssignmentNode->NodeGuid.ToString());
    Context.Result->SetStringField(TEXT("targetUsage"), TEXT("ParticleUpdateScript"));
    Context.Result->SetNumberField(TEXT("assignmentTargetCount"), AssignmentNode->GetAssignmentTargets().Num());
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Bound Niagara parameter '%s' to source '%s' with a real assignment module."), *ParamName, *SourceBinding));
    Context.SendSuccess(true, TEXT("Niagara parameter binding applied."));
#else
    Context.SendError(TEXT("Niagara stack graph utilities are unavailable in this engine version."), TEXT("NIAGARA_BINDING_UNSUPPORTED"));
#endif
    return true;
}

bool HandleParameterAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("add_user_parameter")) return AddUserParameter(Context);
    if (SubAction == TEXT("set_parameter_value")) return SetParameterValue(Context);
    if (SubAction == TEXT("bind_parameter_to_source")) return BindParameterToSource(Context);
    return false;
}
}
#endif

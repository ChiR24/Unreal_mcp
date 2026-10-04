#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

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

#if MCP_HAS_NIAGARA_INPUT_OVERRIDES
// Points a function input at a parameter, as the stack's Link Inputs menu does, so the input reads it every run.
// Whatever fed the input before (a link, a dynamic input) goes first: the engine asserts on an override pin with links.
static void LinkInputToParameter(UNiagaraNodeFunctionCall& Function, const FString& InputName, const FNiagaraTypeDefinition& Type, const FNiagaraVariable& Source)
{
    UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
        Function, FNiagaraParameterHandle(FName(*Function.GetFunctionName()), FName(*InputName)), Type, FGuid(), FGuid());
    TArray<UEdGraphNode*> Feeding;
    for (UEdGraphPin* Linked : OverridePin.LinkedTo)
    {
        Feeding.AddUnique(Linked->GetOwningNode());
    }
    OverridePin.BreakAllPinLinks();
    for (UEdGraphNode* Node : Feeding)
    {
        if (!Node->Pins.ContainsByPredicate([](const UEdGraphPin* Pin) { return Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0; }))
        {
            Node->DestroyNode();
        }
    }
#if ENGINE_MINOR_VERSION >= 6
    const TSet<FNiagaraVariableBase> Known{Source};
    FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(OverridePin, Source, Known);
#else
    const TSet<FNiagaraVariable> Known{Source};
    FNiagaraStackGraphUtilities::SetLinkedValueHandleForFunctionInput(OverridePin, FNiagaraParameterHandle(Source.GetName()), Known);
#endif
}
#endif

// A module input as get_niagara_info lists it (InitializeParticle.Color) is linked to sourceBinding where it is; any
// other name gets a Set Variables module in particle update that writes it from sourceBinding every frame. Both read
// the source through a link: a source name given to Set Variables as a value is linked only for engine constants.
static bool BindParameterToSource(FActionContext& Context)
{
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"));
    const FString SourceBinding = GetJsonStringField(Context.Payload, TEXT("sourceBinding"));
    if (Context.SystemPath.IsEmpty() || ParamName.IsEmpty() || SourceBinding.IsEmpty())
    {
        Context.SendError(Context.SystemPath.IsEmpty() ? TEXT("Missing 'systemPath' (or 'assetPath'): the Niagara System.") : TEXT("Missing 'parameterName' or 'sourceBinding'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!ValidateNiagaraIdentifier(Context, ParamName, TEXT("parameterName"), true) || !ValidateNiagaraIdentifier(Context, SourceBinding, TEXT("sourceBinding"), true))
    {
        return true;
    }
#if MCP_HAS_NIAGARA_INPUT_OVERRIDES
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(Handle);
    UNiagaraGraph* Graph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
    if (!Graph)
    {
        Context.SendError(TEXT("Emitter has no Niagara graph source."), TEXT("NIAGARA_GRAPH_MISSING"));
        return true;
    }
    FString NotAnInput;
    FString NotAnInputCode;
    UNiagaraNodeFunctionCall* Function = ResolveDynamicInputTargetNode(Context, Graph, FString(), ParamName, NotAnInput, NotAnInputCode);
    FNiagaraVariable Input;
    const bool bModuleInput = Function && FindModuleInput(Function, ParamName, Input);
    // "InitializeParticle.Colour" names a module but none of its inputs: a typo, not a variable to create.
    int32 Dot = INDEX_NONE;
    const FString Prefix = ParamName.FindChar(TEXT('.'), Dot) ? ParamName.Left(Dot) : FString();
    if (!bModuleInput && !Prefix.IsEmpty() && Graph->Nodes.ContainsByPredicate([&Prefix](UEdGraphNode* Node)
        {
            UNiagaraNodeFunctionCall* Call = Cast<UNiagaraNodeFunctionCall>(Node);
            return Call && Call->GetFunctionName().Equals(Prefix, ESearchCase::IgnoreCase);
        }))
    {
        Context.SendError(FString::Printf(TEXT("'%s' names a module of this emitter but none of its inputs. %s"), *ParamName, *NotAnInput), TEXT("INPUT_NOT_FOUND"));
        return true;
    }
    FString InputName = ParamName;
    FNiagaraTypeDefinition Type = ResolveNiagaraTypeByName(GetJsonStringField(Context.Payload, TEXT("parameterType"), TEXT("Float")));
    if (bModuleInput)
    {
        InputName = Input.GetName().ToString();
        InputName.RemoveFromStart(TEXT("Module."));
        Type = Input.GetType();
    }
    // An unknown user parameter would compile to its type's default without a word.
    if (SourceBinding.StartsWith(TEXT("User.")))
    {
        const FNiagaraVariableWithOffset* User = System->GetExposedParameters().ReadParameterVariables().FindByPredicate(
            [&SourceBinding](const FNiagaraVariableWithOffset& Var) { return Var.GetName().ToString() == SourceBinding; });
        if (!User || User->GetType() != Type)
        {
            Context.SendError(FString::Printf(TEXT("User parameter '%s' %s; '%s' takes a %s (add_user_parameter makes one)."), *SourceBinding,
                User ? *FString::Printf(TEXT("is a %s"), *User->GetType().GetName()) : TEXT("does not exist"), *ParamName, *Type.GetName()),
                User ? TEXT("PARAM_TYPE_MISMATCH") : TEXT("PARAM_NOT_FOUND"));
            return true;
        }
    }
    Graph->Modify();
    if (!bModuleInput)
    {
        UNiagaraNodeOutput* TargetOutput = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node); OutputNode && OutputNode->GetUsage() == ENiagaraScriptUsage::ParticleUpdateScript)
            {
                TargetOutput = OutputNode;
                break;
            }
        }
        UNiagaraNodeAssignment* AssignmentNode = TargetOutput ? FNiagaraStackGraphUtilities::AddParameterModuleToStack(
            TArray<FNiagaraVariable>{FNiagaraVariable(Type, FName(*ParamName))}, *TargetOutput, INDEX_NONE, TArray<FString>{FString()}) : nullptr;
        if (!AssignmentNode)
        {
            Context.SendError(FString::Printf(TEXT("'%s' is not a module input (%s), and no Set Variables module could be added to particle update."), *ParamName, *NotAnInput), TEXT("NIAGARA_BINDING_FAILED"));
            return true;
        }
        AssignmentNode->RefreshFromExternalChanges();
        AssignmentNode->UpdateUsageBitmaskFromOwningScript();
        Function = AssignmentNode;
    }
    LinkInputToParameter(*Function, InputName, Type, FNiagaraVariable(Type, FName(*SourceBinding)));
    Graph->NotifyGraphChanged();
    System->RequestCompile(false);
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetBoolField(TEXT("bindingApplied"), true);
    Context.Result->SetBoolField(TEXT("assignmentModuleAdded"), !bModuleInput);
    Context.Result->SetStringField(TEXT("parameterName"), ParamName);
    Context.Result->SetStringField(TEXT("sourceBinding"), SourceBinding);
    Context.Result->SetStringField(TEXT("linkedInput"), Function->GetFunctionName() + TEXT(".") + InputName);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("%s.%s now reads '%s'."), *Function->GetFunctionName(), *InputName, *SourceBinding));
    Context.SendSuccess(true, TEXT("Niagara parameter binding applied."));
#else
    Context.SendError(TEXT("Linking a Niagara input to a parameter needs Unreal Engine 5.3 or later."), TEXT("NIAGARA_BINDING_UNSUPPORTED"));
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

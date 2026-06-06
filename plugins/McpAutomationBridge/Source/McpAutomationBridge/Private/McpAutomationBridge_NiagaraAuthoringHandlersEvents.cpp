#include "McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
namespace McpNiagaraAuthoringHandlers
{
static bool RequireEventName(FActionContext& Context, FString& EventName)
{
    EventName = GetJsonStringField(Context.Payload, TEXT("eventName"));
    if (EventName.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'eventName'."), TEXT("INVALID_ARGUMENT"));
        return false;
    }
    return ValidateNiagaraIdentifier(Context, EventName, TEXT("eventName"), false);
}

static FString EventGeneratorModulePath(const FString& EventType)
{
    if (EventType.Equals(TEXT("Collision"), ESearchCase::IgnoreCase))
    {
        return TEXT("/Niagara/Modules/Events/GenerateCollisionEvent.GenerateCollisionEvent");
    }
    if (EventType.Equals(TEXT("Death"), ESearchCase::IgnoreCase))
    {
        return TEXT("/Niagara/Modules/Events/GenerateDeathEvent.GenerateDeathEvent");
    }
    return TEXT("/Niagara/Modules/Events/GenerateLocationEvent.GenerateLocationEvent");
}

static bool AddEventGenerator(FActionContext& Context)
{
    FString EventName;
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!RequireEventName(Context, EventName) || !LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    const FString EventType = GetJsonStringField(Context.Payload, TEXT("eventType"), TEXT("Location"));
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        EventGeneratorModulePath(EventType),
        ENiagaraScriptUsage::ParticleUpdateScript,
        FString::Printf(TEXT("Generate%sEvent"), *EventType));
    const bool bParameterAdded = AddOrSetBoolUserParameter(System, FString::Printf(TEXT("MCP_EventGenerator_%s"), *EventName), true);
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("eventName"), EventName);
    Context.Result->SetStringField(TEXT("eventType"), TEXT("Generator"));
    Context.Result->SetBoolField(TEXT("moduleAdded"), NewModule != nullptr);
    Context.Result->SetBoolField(TEXT("eventGeneratorAdded"), NewModule != nullptr || bParameterAdded);
    Context.Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added event generator '%s'."), *EventName));
    Context.SendSuccess(true, TEXT("Event generator added."));
    return true;
}

static bool AddEventReceiver(FActionContext& Context)
{
    FString EventName;
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!RequireEventName(Context, EventName) || !LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    const bool bSpawnOnEvent = GetJsonBoolField(Context.Payload, TEXT("spawnOnEvent"), false);
    const double EventSpawnCount = GetJsonNumberField(Context.Payload, TEXT("eventSpawnCount"), 1.0);
    bool bEventHandlerAdded = false;
    bool bEventGraphCreated = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FVersionedNiagaraEmitter VersionedEmitter = Handle->GetInstance();
    UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
#else
    UNiagaraEmitter* Emitter = Handle->GetInstance();
#endif
    if (Emitter)
    {
        UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(Handle);
        if (!ScriptSource || !ScriptSource->NodeGraph)
        {
            Context.SendError(TEXT("Emitter graph source is not initialized."), TEXT("NIAGARA_EMITTER_INIT_FAILED"));
            return true;
        }
        Emitter->Modify();
        FNiagaraEventScriptProperties EventProps;
        EventProps.Script = NewObject<UNiagaraScript>(Emitter, MakeUniqueObjectName(Emitter, UNiagaraScript::StaticClass(), TEXT("MCPEventScript")), RF_Transactional);
        if (EventProps.Script)
        {
            EventProps.Script->SetUsage(ENiagaraScriptUsage::ParticleEventScript);
            EventProps.Script->SetUsageId(FGuid::NewGuid());
            EventProps.Script->SetLatestSource(ScriptSource);
            EventProps.SourceEventName = FName(*EventName);
            EventProps.SpawnNumber = static_cast<uint32>(FMath::Max(0.0, EventSpawnCount));
            EventProps.ExecutionMode = bSpawnOnEvent ? EScriptExecutionMode::SpawnedParticles : EScriptExecutionMode::EveryParticle;
            bEventGraphCreated = EnsureScriptOutputGraph(ScriptSource, ENiagaraScriptUsage::ParticleEventScript, EventProps.Script->GetUsageId());
            if (!bEventGraphCreated)
            {
                Context.SendError(TEXT("Failed to create Niagara event handler graph."), TEXT("NIAGARA_GRAPH_CREATE_FAILED"));
                return true;
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            Emitter->AddEventHandler(EventProps, VersionedEmitter.Version);
#else
            Emitter->AddEventHandler(EventProps);
#endif
            bEventHandlerAdded = true;
        }
    }
    const bool bParameterAdded = AddOrSetBoolUserParameter(System, FString::Printf(TEXT("MCP_EventReceiver_%s"), *EventName), true);
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("eventName"), EventName);
    Context.Result->SetStringField(TEXT("eventType"), TEXT("Receiver"));
    Context.Result->SetBoolField(TEXT("spawnOnEvent"), bSpawnOnEvent);
    Context.Result->SetBoolField(TEXT("eventHandlerAdded"), bEventHandlerAdded);
    Context.Result->SetBoolField(TEXT("eventGraphCreated"), bEventGraphCreated);
    Context.Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added event receiver '%s'."), *EventName));
    Context.SendSuccess(true, TEXT("Event receiver added."));
    return true;
}

static bool ConfigureEventPayload(FActionContext& Context)
{
    FString EventName;
    if (Context.SystemPath.IsEmpty() || !RequireEventName(Context, EventName))
    {
        if (Context.SystemPath.IsEmpty()) Context.SendError(TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!UEditorAssetLibrary::DoesAssetExist(Context.SystemPath))
    {
        Context.SendError(FString::Printf(TEXT("Niagara system asset not found: %s"), *Context.SystemPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
    const TArray<TSharedPtr<FJsonValue>>* PayloadArray = nullptr;
    TArray<FString> PayloadAttributes;
    int32 AddedPayloadParameters = 0;
    if (Context.Payload->TryGetArrayField(TEXT("eventPayload"), PayloadArray))
    {
        if (PayloadArray->Num() > 32)
        {
            Context.SendError(FString::Printf(TEXT("'eventPayload' has %d entries. Maximum allowed is %d."), PayloadArray->Num(), 32), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        for (const TSharedPtr<FJsonValue>& Item : *PayloadArray)
        {
            const TSharedPtr<FJsonObject>* AttrObj;
            if (Item->TryGetObject(AttrObj) && AttrObj->IsValid())
            {
                const FString AttrName = GetJsonStringField(*AttrObj, TEXT("name"));
                const FString AttrType = GetJsonStringField(*AttrObj, TEXT("type"));
                if (!AttrName.IsEmpty() && ValidateNiagaraIdentifier(Context, AttrName, TEXT("eventPayload.name"), false))
                {
                    PayloadAttributes.Add(FString::Printf(TEXT("%s:%s"), *AttrName, *AttrType));
                    FNiagaraVariable Param(ResolveNiagaraTypeByName(AttrType), FName(*FString::Printf(TEXT("MCP_EventPayload_%s_%s"), *EventName, *AttrName)));
                    System->GetExposedParameters().AddParameter(Param, true);
                    AddedPayloadParameters += System->GetExposedParameters().FindParameterVariable(Param) ? 1 : 0;
                }
            }
        }
    }
    if (PayloadAttributes.Num() == 0)
    {
        FNiagaraVariable Param(FNiagaraTypeDefinition::GetFloatDef(), FName(*FString::Printf(TEXT("MCP_EventPayload_%s_Default"), *EventName)));
        System->GetExposedParameters().AddParameter(Param, true);
        AddedPayloadParameters += System->GetExposedParameters().FindParameterVariable(Param) ? 1 : 0;
    }
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("eventName"), EventName);
    Context.Result->SetNumberField(TEXT("payloadAttributeCount"), PayloadAttributes.Num());
    Context.Result->SetNumberField(TEXT("payloadParametersAdded"), AddedPayloadParameters);
    Context.Result->SetBoolField(TEXT("eventPayloadConfigured"), AddedPayloadParameters > 0);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured event payload for '%s' with %d attributes."), *EventName, PayloadAttributes.Num()));
    Context.SendSuccess(true, TEXT("Event payload configured."));
    return true;
}

bool HandleEventAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("add_event_generator")) return AddEventGenerator(Context);
    if (SubAction == TEXT("add_event_receiver")) return AddEventReceiver(Context);
    if (SubAction == TEXT("configure_event_payload")) return ConfigureEventPayload(Context);
    return false;
}
}
#endif

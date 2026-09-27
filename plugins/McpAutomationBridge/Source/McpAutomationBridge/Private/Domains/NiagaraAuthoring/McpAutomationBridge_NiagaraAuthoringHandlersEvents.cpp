#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

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
    if (!NewModule)
    {
        Context.SendError(FString::Printf(TEXT("Could not add the Generate%sEvent module to emitter '%s'."), *EventType, *Context.EmitterName),
            TEXT("NIAGARA_MODULE_ADD_FAILED"));
        return true;
    }
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("eventName"), EventName);
    Context.Result->SetStringField(TEXT("eventType"), TEXT("Generator"));
    Context.Result->SetBoolField(TEXT("moduleAdded"), true);
    Context.Result->SetBoolField(TEXT("eventGeneratorAdded"), true);
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
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("eventName"), EventName);
    Context.Result->SetStringField(TEXT("eventType"), TEXT("Receiver"));
    Context.Result->SetBoolField(TEXT("spawnOnEvent"), bSpawnOnEvent);
    Context.Result->SetBoolField(TEXT("eventHandlerAdded"), bEventHandlerAdded);
    Context.Result->SetBoolField(TEXT("eventGraphCreated"), bEventGraphCreated);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added event receiver '%s'."), *EventName));
    Context.SendSuccess(true, TEXT("Event receiver added."));
    return true;
}

bool HandleEventAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("add_event_generator")) return AddEventGenerator(Context);
    if (SubAction == TEXT("add_event_receiver")) return AddEventReceiver(Context);
    return false;
}
}

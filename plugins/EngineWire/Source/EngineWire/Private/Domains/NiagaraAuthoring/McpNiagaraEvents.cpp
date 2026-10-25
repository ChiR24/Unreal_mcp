#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"

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

// The engine's generator module for an event type; empty for any other type. An unknown type made a location
// generator, a misspelt Death included.
static FString EventGeneratorModulePath(FString& InOutEventType)
{
    for (const TCHAR* Type : {TEXT("Location"), TEXT("Collision"), TEXT("Death")})
    {
        if (InOutEventType.Equals(Type, ESearchCase::IgnoreCase))
        {
            InOutEventType = Type;
            return FString::Printf(TEXT("/Niagara/Modules/Events/Generate%sEvent.Generate%sEvent"), Type, Type);
        }
    }
    return FString();
}

// The event a generator module writes, which a receiver's eventName must match: its write node's EventName, else
// that node's data set name. It lives in the module asset, so no eventName given here can rename it.
static FString GeneratedEventName(const UNiagaraNodeFunctionCall* Module)
{
    const UNiagaraScriptSource* Source = Module && Module->FunctionScript
        ? Cast<UNiagaraScriptSource>(Module->FunctionScript->GetLatestSource()) : nullptr;
    if (!Source || !Source->NodeGraph)
    {
        return FString();
    }
    for (const UEdGraphNode* Node : Source->NodeGraph->Nodes)
    {
        const FNameProperty* Prop = Node && Node->GetClass()->GetName() == TEXT("NiagaraNodeWriteDataSet")
            ? CastField<FNameProperty>(Node->GetClass()->FindPropertyByName(TEXT("EventName"))) : nullptr;
        const FStructProperty* DataSet = Prop ? CastField<FStructProperty>(Node->GetClass()->FindPropertyByName(TEXT("DataSet"))) : nullptr;
        const FName Name = Prop ? Prop->GetPropertyValue_InContainer(Node) : NAME_None;
        if (!Name.IsNone() || DataSet)
        {
            return Name.IsNone() ? DataSet->ContainerPtrToValuePtr<FNiagaraDataSetID>(Node)->Name.ToString() : Name.ToString();
        }
    }
    return FString();
}

static bool AddEventGenerator(FActionContext& Context)
{
    // eventName is optional here: the generator's event is named by its module, and the name given was answered as
    // the generator's while the asset emitted another.
    const FString EventName = GetJsonStringField(Context.Payload, TEXT("eventName"));
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    FString EventType = GetJsonStringField(Context.Payload, TEXT("eventType"), TEXT("Location"));
    const FString ModulePath = EventGeneratorModulePath(EventType);
    if (ModulePath.IsEmpty())
    {
        Context.SendError(FString::Printf(TEXT("eventType must be Location, Collision or Death; got '%s'. Nothing was changed."), *EventType),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(
        Handle,
        ModulePath,
        ENiagaraScriptUsage::ParticleUpdateScript,
        FString::Printf(TEXT("Generate%sEvent"), *EventType));
    if (!NewModule)
    {
        Context.SendError(FString::Printf(TEXT("Could not add the Generate%sEvent module to emitter '%s'."), *EventType, *Context.EmitterName),
            TEXT("NIAGARA_MODULE_ADD_FAILED"));
        return true;
    }
    MarkDirtyAndVerify(Context, System);
    const FString Emitted = GeneratedEventName(NewModule);
    if (!Emitted.IsEmpty())
    {
        Context.Result->SetStringField(TEXT("eventName"), Emitted);
    }
    Context.Result->SetStringField(TEXT("eventType"), EventType);
    Context.Result->SetStringField(TEXT("moduleName"), NewModule->GetFunctionName());
    Context.Result->SetBoolField(TEXT("moduleAdded"), true);
    Context.Result->SetBoolField(TEXT("eventGeneratorAdded"), true);
    if (!EventName.IsEmpty() && !Emitted.IsEmpty() && !EventName.Equals(Emitted, ESearchCase::IgnoreCase))
    {
        TArray<TSharedPtr<FJsonValue>> Warnings;
        Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("The generator emits '%s', set in the engine's Generate%sEvent "
            "module; eventName '%s' does not rename it. Give add_event_receiver eventName '%s'."), *Emitted, *EventType, *EventName, *Emitted)));
        Context.Result->SetArrayField(TEXT("warnings"), Warnings);
    }
    Context.SendSuccess(true, Emitted.IsEmpty() ? FString(TEXT("Event generator added."))
                                                : FString::Printf(TEXT("Event generator added; it emits '%s'."), *Emitted));
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
    // The engine reads SpawnNumber only for a handler that spawns particles: a count sent without spawnOnEvent did nothing.
    Context.Result->SetNumberField(TEXT("eventSpawnCount"), bSpawnOnEvent ? FMath::Max(0.0, EventSpawnCount) : 0.0);
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

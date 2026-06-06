#include "McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
namespace McpNiagaraAuthoringHandlers
{
static bool EnableGpuSimulation(FActionContext& Context)
{
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    UNiagaraEmitter* Emitter = Handle->GetInstance().Emitter;
    if (Emitter)
    {
        if (MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = MCP_GET_LATEST_EMITTER_DATA(Emitter))
        {
            EmitterData->SimTarget = ENiagaraSimTarget::GPUComputeSim;
        }
    }
#else
    UNiagaraEmitter* Emitter = Handle->GetInstance();
    if (Emitter)
    {
        Emitter->SimTarget = ENiagaraSimTarget::GPUComputeSim;
    }
#endif
    const bool bFixedBounds = GetJsonBoolField(Context.Payload, TEXT("fixedBoundsEnabled"), false);
    const bool bDeterministic = GetJsonBoolField(Context.Payload, TEXT("deterministicEnabled"), false);
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetBoolField(TEXT("gpuEnabled"), true);
    Context.Result->SetBoolField(TEXT("fixedBoundsEnabled"), bFixedBounds);
    Context.Result->SetBoolField(TEXT("deterministicEnabled"), bDeterministic);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Enabled GPU simulation for emitter '%s'."), *Context.EmitterName));
    Context.SendSuccess(true, TEXT("GPU simulation enabled."));
    return true;
}

static bool AddSimulationStage(FActionContext& Context)
{
    const FString StageName = GetJsonStringField(Context.Payload, TEXT("stageName"));
    if (StageName.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'stageName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!ValidateNiagaraIdentifier(Context, StageName, TEXT("stageName"), false))
    {
        return true;
    }
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    const FString IterationSource = GetJsonStringField(Context.Payload, TEXT("stageIterationSource"), TEXT("Particles"));
    bool bSimulationStageAdded = false;
    bool bSimulationStageGraphCreated = false;
    int32 SimulationStageCount = 0;
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
        UNiagaraSimulationStageGeneric* NewStage = NewObject<UNiagaraSimulationStageGeneric>(Emitter, NAME_None, RF_Transactional);
        if (NewStage)
        {
            NewStage->SimulationStageName = FName(*StageName);
            NewStage->IterationSource = IterationSource.Equals(TEXT("DataInterface"), ESearchCase::IgnoreCase) ? ENiagaraIterationSource::DataInterface : ENiagaraIterationSource::Particles;
            NewStage->Script = NewObject<UNiagaraScript>(NewStage, MakeUniqueObjectName(NewStage, UNiagaraScript::StaticClass(), TEXT("MCPSimulationStageScript")), RF_Transactional);
            if (NewStage->Script)
            {
                NewStage->Script->SetUsage(ENiagaraScriptUsage::ParticleSimulationStageScript);
                NewStage->Script->SetUsageId(FGuid::NewGuid());
                NewStage->Script->SetLatestSource(ScriptSource);
                bSimulationStageGraphCreated = EnsureScriptOutputGraph(ScriptSource, ENiagaraScriptUsage::ParticleSimulationStageScript, NewStage->Script->GetUsageId());
                if (!bSimulationStageGraphCreated)
                {
                    Context.SendError(TEXT("Failed to create Niagara simulation stage graph."), TEXT("NIAGARA_GRAPH_CREATE_FAILED"));
                    return true;
                }
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            Emitter->AddSimulationStage(NewStage, VersionedEmitter.Version);
            if (FVersionedNiagaraEmitterData* EmitterData = Emitter->GetEmitterData(VersionedEmitter.Version))
            {
                SimulationStageCount = EmitterData->GetSimulationStages().Num();
            }
#else
            Emitter->AddSimulationStage(NewStage);
            SimulationStageCount = 1;
#endif
            bSimulationStageAdded = bSimulationStageGraphCreated;
        }
    }
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("stageName"), StageName);
    Context.Result->SetStringField(TEXT("iterationSource"), IterationSource);
    Context.Result->SetBoolField(TEXT("simulationStageAdded"), bSimulationStageAdded);
    Context.Result->SetBoolField(TEXT("simulationStageGraphCreated"), bSimulationStageGraphCreated);
    Context.Result->SetNumberField(TEXT("simulationStageCount"), SimulationStageCount);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added simulation stage '%s'."), *StageName));
    Context.SendSuccess(true, TEXT("Simulation stage added."));
    return true;
}

bool HandleSimulationAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("enable_gpu_simulation")) return EnableGpuSimulation(Context);
    if (SubAction == TEXT("add_simulation_stage")) return AddSimulationStage(Context);
    return false;
}
}
#endif

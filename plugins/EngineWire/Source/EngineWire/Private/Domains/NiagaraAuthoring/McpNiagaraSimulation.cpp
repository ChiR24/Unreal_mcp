#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"

namespace McpNiagaraAuthoringHandlers
{
static bool EnableGpuSimulation(FActionContext& Context)
{
    FEmitterTarget Target;
    if (!LoadEmitterTarget(Context, Target))
    {
        return true;
    }
    UNiagaraSystem* System = Target.System;
    Target.EmitterData->SimTarget = ENiagaraSimTarget::GPUComputeSim;
    // fixedBoundsEnabled/deterministicEnabled set the system's bFixedBounds/bDeterminism when given (by reflection:
    // their C++ access differs across engine versions); the reply reads both back. They were only echoed before.
    System->Modify();
    const auto ApplySystemFlag = [&](const TCHAR* Field, const TCHAR* PropertyName)
    {
        const FBoolProperty* Property = FindFProperty<FBoolProperty>(UNiagaraSystem::StaticClass(), PropertyName);
        bool bValue = false;
        if (Property && Context.Payload->TryGetBoolField(Field, bValue))
        {
            Property->SetPropertyValue_InContainer(System, bValue);
        }
        Context.Result->SetBoolField(Field, Property && Property->GetPropertyValue_InContainer(System));
    };
    ApplySystemFlag(TEXT("fixedBoundsEnabled"), TEXT("bFixedBounds"));
    ApplySystemFlag(TEXT("deterministicEnabled"), TEXT("bDeterminism"));
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetBoolField(TEXT("gpuEnabled"), Target.EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim);
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
    // An unknown source, DirectSet included, made a particle stage while the reply named the one asked for.
    FString IterationSource = GetJsonStringField(Context.Payload, TEXT("stageIterationSource"), TEXT("Particles"));
    ENiagaraIterationSource Source = ENiagaraIterationSource::Particles;
    if (IterationSource.Equals(TEXT("DataInterface"), ESearchCase::IgnoreCase))
    {
        Source = ENiagaraIterationSource::DataInterface;
        IterationSource = TEXT("DataInterface");
    }
#if MCP_HAS_NIAGARA_ITERATION_DIRECT_SET
    else if (IterationSource.Equals(TEXT("DirectSet"), ESearchCase::IgnoreCase))
    {
        Source = ENiagaraIterationSource::DirectSet;
        IterationSource = TEXT("DirectSet");
    }
#endif
    else if (!IterationSource.Equals(TEXT("Particles"), ESearchCase::IgnoreCase))
    {
        Context.SendError(FString::Printf(TEXT("stageIterationSource must be Particles, DataInterface or DirectSet (UE 5.2 and later); "
            "got '%s'. Nothing was changed."), *IterationSource), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    else
    {
        IterationSource = TEXT("Particles");
    }
    FEmitterTarget Target;
    if (!LoadEmitterTarget(Context, Target))
    {
        return true;
    }
    bool bSimulationStageAdded = false;
    bool bSimulationStageGraphCreated = false;
    int32 SimulationStageCount = 0;
    UNiagaraEmitter* Emitter = Target.Emitter;
    UNiagaraScriptSource* ScriptSource = GetEmitterScriptSource(Target.Handle);
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
        NewStage->IterationSource = Source;
        NewStage->Script = NewObject<UNiagaraScript>(NewStage, MakeUniqueObjectName(NewStage, UNiagaraScript::StaticClass(), TEXT("MCPSimulationStageScript")), RF_Transactional);
        // The engine adds a stage only with its script, which AddSimulationStage reads: a stage without one crashed.
        if (!NewStage->Script)
        {
            Context.SendError(TEXT("Failed to create the simulation stage script."), TEXT("NIAGARA_GRAPH_CREATE_FAILED"));
            return true;
        }
        {
            // The engine keys a stage's script by the stage's merge id, as emitter merging matches them.
            const FStructProperty* MergeIdProp = CastField<FStructProperty>(NewStage->GetClass()->FindPropertyByName(TEXT("MergeId")));
            const FGuid* MergeId = MergeIdProp ? MergeIdProp->ContainerPtrToValuePtr<FGuid>(NewStage) : nullptr;
            NewStage->Script->SetUsage(ENiagaraScriptUsage::ParticleSimulationStageScript);
            NewStage->Script->SetUsageId(MergeId && MergeId->IsValid() ? *MergeId : FGuid::NewGuid());
            NewStage->Script->SetLatestSource(ScriptSource);
            bSimulationStageGraphCreated = EnsureScriptOutputGraph(ScriptSource, ENiagaraScriptUsage::ParticleSimulationStageScript, NewStage->Script->GetUsageId());
            if (!bSimulationStageGraphCreated)
            {
                Context.SendError(TEXT("Failed to create Niagara simulation stage graph."), TEXT("NIAGARA_GRAPH_CREATE_FAILED"));
                return true;
            }
        }
#if MCP_HAS_NIAGARA_VERSIONED_EMITTER
        Emitter->AddSimulationStage(NewStage, Target.VersionedEmitter.Version);
#else
        Emitter->AddSimulationStage(NewStage);
#endif
        SimulationStageCount = Target.EmitterData->GetSimulationStages().Num();
        bSimulationStageAdded = bSimulationStageGraphCreated;
    }
    MarkDirtyAndVerify(Context, Target.System);
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

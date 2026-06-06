#include "McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
namespace McpNiagaraAuthoringHandlers
{
static bool AddNamedModule(
    FActionContext& Context,
    const FString& ModulePath,
    ENiagaraScriptUsage Usage,
    const FString& SuggestedName,
    const FString& ModuleName,
    const FString& ResponseText)
{
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    UNiagaraNodeFunctionCall* NewModule = AddModuleToEmitterStack(Handle, ModulePath, Usage, SuggestedName);
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("moduleName"), ModuleName);
    Context.Result->SetBoolField(TEXT("moduleAdded"), NewModule != nullptr);
    Context.Result->SetStringField(TEXT("message"), ResponseText);
    return false;
}

static bool AddSpawnRateModule(FActionContext& Context)
{
    const double SpawnRate = GetJsonNumberField(Context.Payload, TEXT("spawnRate"), 100.0);
    if (AddNamedModule(Context, TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate"), ENiagaraScriptUsage::EmitterUpdateScript, TEXT("SpawnRate"), TEXT("SpawnRate"), FString::Printf(TEXT("Added spawn rate module: %.1f particles/sec"), SpawnRate)))
    {
        return true;
    }
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *Context.SystemPath);
    if (System)
    {
        FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
        FNiagaraVariable SpawnRateVar(FNiagaraTypeDefinition::GetFloatDef(), FName(TEXT("SpawnRate")));
        if (UserStore.FindParameterVariable(SpawnRateVar))
        {
            UserStore.SetParameterValue(static_cast<float>(SpawnRate), SpawnRateVar);
        }
    }
    Context.Result->SetNumberField(TEXT("spawnRate"), SpawnRate);
    Context.SendSuccess(true, TEXT("Spawn rate module added."));
    return true;
}

static bool AddSpawnBurstModule(FActionContext& Context)
{
    const double BurstCount = GetJsonNumberField(Context.Payload, TEXT("burstCount"), 10.0);
    const double BurstTime = GetJsonNumberField(Context.Payload, TEXT("burstTime"), 0.0);
    if (AddNamedModule(Context, TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous"), ENiagaraScriptUsage::EmitterSpawnScript, TEXT("SpawnBurst"), TEXT("SpawnBurst"), FString::Printf(TEXT("Added spawn burst module: %d particles at t=%.2f"), static_cast<int>(BurstCount), BurstTime)))
    {
        return true;
    }
    Context.Result->SetNumberField(TEXT("burstCount"), BurstCount);
    Context.Result->SetNumberField(TEXT("burstTime"), BurstTime);
    Context.SendSuccess(true, TEXT("Spawn burst module added."));
    return true;
}

static bool AddSpawnPerUnitModule(FActionContext& Context)
{
    const double SpawnPerUnit = GetJsonNumberField(Context.Payload, TEXT("spawnPerUnit"), 1.0);
    if (AddNamedModule(Context, TEXT("/Niagara/Modules/Emitter/SpawnPerUnit.SpawnPerUnit"), ENiagaraScriptUsage::EmitterUpdateScript, TEXT("SpawnPerUnit"), TEXT("SpawnPerUnit"), FString::Printf(TEXT("Added spawn per unit module: %.1f particles/unit"), SpawnPerUnit)))
    {
        return true;
    }
    Context.Result->SetNumberField(TEXT("spawnPerUnit"), SpawnPerUnit);
    Context.SendSuccess(true, TEXT("Spawn per unit module added."));
    return true;
}

static bool AddInitializeParticleModule(FActionContext& Context)
{
    const double Lifetime = GetJsonNumberField(Context.Payload, TEXT("lifetime"), 2.0);
    const double Mass = GetJsonNumberField(Context.Payload, TEXT("mass"), 1.0);
    if (AddNamedModule(Context, TEXT("/Niagara/Modules/Spawn/Initialization/InitializeParticle.InitializeParticle"), ENiagaraScriptUsage::ParticleSpawnScript, TEXT("InitializeParticle"), TEXT("InitializeParticle"), FString::Printf(TEXT("Added initialize particle module: lifetime=%.2fs, mass=%.2f"), Lifetime, Mass)))
    {
        return true;
    }
    Context.Result->SetNumberField(TEXT("lifetime"), Lifetime);
    Context.Result->SetNumberField(TEXT("mass"), Mass);
    Context.SendSuccess(true, TEXT("Initialize particle module added."));
    return true;
}

static bool AddParticleStateModule(FActionContext& Context)
{
    if (AddNamedModule(Context, TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("ParticleState"), TEXT("ParticleState"), TEXT("Added particle state module.")))
    {
        return true;
    }
    Context.SendSuccess(true, TEXT("Particle state module added."));
    return true;
}

bool HandleSpawnModuleAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("add_spawn_rate_module")) return AddSpawnRateModule(Context);
    if (SubAction == TEXT("add_spawn_burst_module")) return AddSpawnBurstModule(Context);
    if (SubAction == TEXT("add_spawn_per_unit_module")) return AddSpawnPerUnitModule(Context);
    if (SubAction == TEXT("add_initialize_particle_module")) return AddInitializeParticleModule(Context);
    if (SubAction == TEXT("add_particle_state_module")) return AddParticleStateModule(Context);
    return false;
}
}
#endif

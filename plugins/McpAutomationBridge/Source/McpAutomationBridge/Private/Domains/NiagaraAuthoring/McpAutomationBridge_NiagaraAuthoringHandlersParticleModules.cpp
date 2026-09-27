#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

namespace McpNiagaraAuthoringHandlers
{
// The fixed-module doors insert one stock module with its default inputs; set_parameter_value
// tunes them afterwards (e.g. parameterName "SpawnRate.SpawnRate"). Only the selectors that pick
// the module script (forceType, velocityMode) are read here.
static FString ForceModulePath(const FString& ForceType)
{
    // DragForce is deprecated on UE 5.7; Drag is its successor.
    if (ForceType.Equals(TEXT("Drag"), ESearchCase::IgnoreCase))
    {
        return McpPreferredModulePath(TEXT("/Niagara/Modules/Update/Forces/Drag.Drag"),
                                      TEXT("/Niagara/Modules/Update/Forces/DragForce.DragForce"));
    }
    if (ForceType.Equals(TEXT("Wind"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Update/Forces/WindForce.WindForce");
    if (ForceType.Equals(TEXT("Curl"), ESearchCase::IgnoreCase) || ForceType.Equals(TEXT("CurlNoise"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Update/Forces/CurlNoiseForce.CurlNoiseForce");
    if (ForceType.Equals(TEXT("Vortex"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Update/Forces/VortexForce.VortexForce");
    if (ForceType.Equals(TEXT("PointAttraction"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Update/Forces/PointAttractionForce.PointAttractionForce");
    return TEXT("/Niagara/Modules/Update/Forces/GravityForce.GravityForce");
}

static FString VelocityModulePath(const FString& VelocityMode)
{
    if (VelocityMode.Equals(TEXT("Cone"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityInCone.AddVelocityInCone");
    if (VelocityMode.Equals(TEXT("FromPoint"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityFromPoint.AddVelocityFromPoint");
    return TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity");
}

struct FFixedModule
{
    const TCHAR* SubAction;
    const TCHAR* ModulePath; // null: resolved from a selector below
    ENiagaraScriptUsage Usage;
    const TCHAR* ModuleName;
};

static const FFixedModule FixedModules[] = {
    {TEXT("add_spawn_rate_module"), TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate"), ENiagaraScriptUsage::EmitterUpdateScript, TEXT("SpawnRate")},
    {TEXT("add_spawn_burst_module"), TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous"), ENiagaraScriptUsage::EmitterSpawnScript, TEXT("SpawnBurst")},
    {TEXT("add_spawn_per_unit_module"), TEXT("/Niagara/Modules/Emitter/SpawnPerUnit.SpawnPerUnit"), ENiagaraScriptUsage::EmitterUpdateScript, TEXT("SpawnPerUnit")},
    {TEXT("add_initialize_particle_module"), nullptr, ENiagaraScriptUsage::ParticleSpawnScript, TEXT("InitializeParticle")},
    {TEXT("add_particle_state_module"), TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("ParticleState")},
    {TEXT("add_force_module"), nullptr, ENiagaraScriptUsage::ParticleUpdateScript, TEXT("Force")},
    {TEXT("add_velocity_module"), nullptr, ENiagaraScriptUsage::ParticleSpawnScript, TEXT("AddVelocity")},
    {TEXT("add_acceleration_module"), TEXT("/Niagara/Modules/Update/Forces/AccelerationForce.AccelerationForce"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("Acceleration Force")},
    {TEXT("add_size_module"), TEXT("/Niagara/Modules/Update/Size/ScaleSpriteSize.ScaleSpriteSize"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("Scale Sprite Size")},
    {TEXT("add_color_module"), TEXT("/Niagara/Modules/Update/Color/Color.Color"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("Color")},
    {TEXT("add_collision_module"), TEXT("/Niagara/Modules/Collision/Collision.Collision"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("Collision")},
    {TEXT("add_kill_particles_module"), TEXT("/Niagara/Modules/Update/Lifetime/KillParticles.KillParticles"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("KillParticles")},
    {TEXT("add_camera_offset_module"), TEXT("/Niagara/Modules/Update/Camera/CameraOffset.CameraOffset"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("CameraOffset")},
};

bool HandleFixedModuleAction(FActionContext& Context, const FString& SubAction)
{
    const FFixedModule* Module = nullptr;
    for (const FFixedModule& Candidate : FixedModules)
    {
        if (SubAction == Candidate.SubAction) { Module = &Candidate; break; }
    }
    if (!Module)
    {
        return false;
    }

    FString ModulePath = Module->ModulePath ? FString(Module->ModulePath) : FString();
    FString ModuleName = Module->ModuleName;
    if (SubAction == TEXT("add_force_module"))
    {
        const FString ForceType = GetJsonStringField(Context.Payload, TEXT("forceType"), TEXT("Gravity"));
        ModulePath = ForceModulePath(ForceType);
        ModuleName = ForceType + TEXT("Force");
        Context.Result->SetStringField(TEXT("forceType"), ForceType);
    }
    else if (SubAction == TEXT("add_velocity_module"))
    {
        const FString VelocityMode = GetJsonStringField(Context.Payload, TEXT("velocityMode"), TEXT("Linear"));
        ModulePath = VelocityModulePath(VelocityMode);
        Context.Result->SetStringField(TEXT("velocityMode"), VelocityMode);
    }
    else if (SubAction == TEXT("add_initialize_particle_module"))
    {
        // The non-V2 InitializeParticle is deprecated on UE 5.7.
        ModulePath = McpPreferredModulePath(
            TEXT("/Niagara/Modules/Spawn/Initialization/V2/InitializeParticle.InitializeParticle"),
            TEXT("/Niagara/Modules/Spawn/Initialization/InitializeParticle.InitializeParticle"));
    }

    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    // Recorded so an unmet-dependency report can name the actual dependency.
    Context.Result->SetStringField(TEXT("moduleScriptPath"), ModulePath);
    if (!AddModuleToEmitterStack(Handle, ModulePath, Module->Usage, ModuleName))
    {
        Context.SendError(FString::Printf(TEXT("Failed to add the %s module to the emitter stack."), *ModuleName), TEXT("CREATE_FAILED"));
        return true;
    }
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("moduleName"), ModuleName);
    Context.Result->SetBoolField(TEXT("moduleAdded"), true);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(
        TEXT("Added the %s module with its default inputs; set them with set_parameter_value (parameterName \"<Module>.<Input>\")."), *ModuleName));
    Context.SendSuccess(true, FString::Printf(TEXT("%s module added."), *ModuleName));
    return true;
}
}

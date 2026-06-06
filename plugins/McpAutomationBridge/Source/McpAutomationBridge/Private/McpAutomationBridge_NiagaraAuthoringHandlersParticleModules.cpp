#include "McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
namespace McpNiagaraAuthoringHandlers
{
static bool AddModuleAndVerify(
    FActionContext& Context,
    const FString& ModulePath,
    ENiagaraScriptUsage Usage,
    const FString& SuggestedName,
    UNiagaraSystem*& System)
{
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return false;
    }
    Context.Result->SetBoolField(TEXT("moduleAdded"), AddModuleToEmitterStack(Handle, ModulePath, Usage, SuggestedName) != nullptr);
    MarkDirtyAndVerify(Context, System);
    return true;
}

static FString ForceModulePath(const FString& ForceType)
{
    if (ForceType.Equals(TEXT("Drag"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Update/Forces/DragForce.DragForce");
    if (ForceType.Equals(TEXT("Wind"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Update/Forces/WindForce.WindForce");
    if (ForceType.Equals(TEXT("Curl"), ESearchCase::IgnoreCase) || ForceType.Equals(TEXT("CurlNoise"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Update/Forces/CurlNoiseForce.CurlNoiseForce");
    if (ForceType.Equals(TEXT("Vortex"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Update/Forces/VortexForce.VortexForce");
    if (ForceType.Equals(TEXT("PointAttraction"), ESearchCase::IgnoreCase)) return TEXT("/Niagara/Modules/Update/Forces/PointAttractionForce.PointAttractionForce");
    return TEXT("/Niagara/Modules/Update/Forces/GravityForce.GravityForce");
}

static bool AddForceModule(FActionContext& Context)
{
    const FString ForceType = GetJsonStringField(Context.Payload, TEXT("forceType"), TEXT("Gravity"));
    const double ForceStrength = GetJsonNumberField(Context.Payload, TEXT("forceStrength"), 980.0);
    UNiagaraSystem* System = nullptr;
    if (!AddModuleAndVerify(Context, ForceModulePath(ForceType), ENiagaraScriptUsage::ParticleUpdateScript, FString::Printf(TEXT("%sForce"), *ForceType), System))
    {
        return true;
    }
    Context.Result->SetStringField(TEXT("moduleName"), FString::Printf(TEXT("Force_%s"), *ForceType));
    Context.Result->SetStringField(TEXT("forceType"), ForceType);
    Context.Result->SetNumberField(TEXT("forceStrength"), ForceStrength);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s force module."), *ForceType));
    Context.SendSuccess(true, TEXT("Force module added."));
    return true;
}

static bool AddVelocityModule(FActionContext& Context)
{
    const FString VelocityMode = GetJsonStringField(Context.Payload, TEXT("velocityMode"), TEXT("Linear"));
    FString ModulePath = TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity");
    if (VelocityMode.Equals(TEXT("Cone"), ESearchCase::IgnoreCase))
    {
        ModulePath = TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityInCone.AddVelocityInCone");
    }
    else if (VelocityMode.Equals(TEXT("FromPoint"), ESearchCase::IgnoreCase))
    {
        ModulePath = TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocityFromPoint.AddVelocityFromPoint");
    }
    UNiagaraSystem* System = nullptr;
    if (!AddModuleAndVerify(Context, ModulePath, ENiagaraScriptUsage::ParticleSpawnScript, TEXT("AddVelocity"), System))
    {
        return true;
    }
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("Velocity"));
    Context.Result->SetStringField(TEXT("velocityMode"), VelocityMode);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added velocity module: mode=%s"), *VelocityMode));
    Context.SendSuccess(true, TEXT("Velocity module added."));
    return true;
}

static bool AddAccelerationModule(FActionContext& Context)
{
    if (Context.SystemPath.IsEmpty() || Context.EmitterName.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const TSharedPtr<FJsonObject>* AccelObj;
    FVector Acceleration = FVector(0, 0, -980);
    if (Context.Payload->TryGetObjectField(TEXT("acceleration"), AccelObj))
    {
        Acceleration = GetVectorFromJson(*AccelObj);
    }
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
    const bool bParameterAdded = AddOrSetVectorUserParameter(System, TEXT("MCP_Acceleration"), Acceleration);
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("Acceleration"));
    Context.Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Context.Result->SetStringField(TEXT("parameterName"), TEXT("MCP_Acceleration"));
    Context.Result->SetStringField(TEXT("message"), TEXT("Configured acceleration module."));
    Context.SendSuccess(true, TEXT("Acceleration module configured."));
    return true;
}

static bool AddSizeModule(FActionContext& Context)
{
    if (Context.SystemPath.IsEmpty() || Context.EmitterName.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
    const FString SizeMode = GetJsonStringField(Context.Payload, TEXT("sizeMode"), TEXT("Uniform"));
    const double UniformSize = GetJsonNumberField(Context.Payload, TEXT("uniformSize"), 10.0);
    const bool bParameterAdded = AddOrSetFloatUserParameter(System, TEXT("MCP_UniformSize"), static_cast<float>(UniformSize));
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("Size"));
    Context.Result->SetStringField(TEXT("sizeMode"), SizeMode);
    Context.Result->SetNumberField(TEXT("uniformSize"), UniformSize);
    Context.Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Context.Result->SetStringField(TEXT("parameterName"), TEXT("MCP_UniformSize"));
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured size module: mode=%s, size=%.1f"), *SizeMode, UniformSize));
    Context.SendSuccess(true, TEXT("Size module configured."));
    return true;
}

static bool AddColorModule(FActionContext& Context)
{
    if (Context.SystemPath.IsEmpty() || Context.EmitterName.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const TSharedPtr<FJsonObject>* ColorObj;
    FLinearColor Color = FLinearColor::White;
    if (Context.Payload->TryGetObjectField(TEXT("color"), ColorObj))
    {
        Color = GetColorFromJson(*ColorObj);
    }
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
    const FString ColorMode = GetJsonStringField(Context.Payload, TEXT("colorMode"), TEXT("Direct"));
    const bool bParameterAdded = AddOrSetColorUserParameter(System, TEXT("MCP_Color"), Color);
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("Color"));
    Context.Result->SetStringField(TEXT("colorMode"), ColorMode);
    Context.Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Context.Result->SetStringField(TEXT("parameterName"), TEXT("MCP_Color"));
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured color module: mode=%s"), *ColorMode));
    Context.SendSuccess(true, TEXT("Color module configured."));
    return true;
}

static bool AddCollisionModule(FActionContext& Context)
{
    UNiagaraSystem* System = nullptr;
    if (!AddModuleAndVerify(Context, TEXT("/Niagara/Modules/Collision/Collision.Collision"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("Collision"), System))
    {
        return true;
    }
    const FString CollisionMode = GetJsonStringField(Context.Payload, TEXT("collisionMode"), TEXT("SceneDepth"));
    const double Restitution = GetJsonNumberField(Context.Payload, TEXT("restitution"), 0.3);
    const double Friction = GetJsonNumberField(Context.Payload, TEXT("friction"), 0.2);
    const bool bDieOnCollision = GetJsonBoolField(Context.Payload, TEXT("dieOnCollision"), false);
    const bool bRestitutionAdded = AddOrSetFloatUserParameter(System, TEXT("MCP_CollisionRestitution"), static_cast<float>(Restitution));
    const bool bFrictionAdded = AddOrSetFloatUserParameter(System, TEXT("MCP_CollisionFriction"), static_cast<float>(Friction));
    const bool bDieOnCollisionAdded = AddOrSetBoolUserParameter(System, TEXT("MCP_DieOnCollision"), bDieOnCollision);
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("Collision"));
    Context.Result->SetStringField(TEXT("collisionMode"), CollisionMode);
    Context.Result->SetNumberField(TEXT("restitution"), Restitution);
    Context.Result->SetNumberField(TEXT("friction"), Friction);
    Context.Result->SetBoolField(TEXT("dieOnCollision"), bDieOnCollision);
    Context.Result->SetBoolField(TEXT("parameterAdded"), bRestitutionAdded && bFrictionAdded && bDieOnCollisionAdded);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured collision module: mode=%s"), *CollisionMode));
    Context.SendSuccess(true, TEXT("Collision module configured."));
    return true;
}

static bool AddKillParticlesModule(FActionContext& Context)
{
    UNiagaraSystem* System = nullptr;
    if (!AddModuleAndVerify(Context, TEXT("/Niagara/Modules/Update/Lifetime/KillParticles.KillParticles"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("KillParticles"), System))
    {
        return true;
    }
    const FString KillCondition = GetJsonStringField(Context.Payload, TEXT("killCondition"), TEXT("Age"));
    const bool bParameterAdded = AddOrSetBoolUserParameter(System, TEXT("MCP_KillParticlesEnabled"), true);
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("KillParticles"));
    Context.Result->SetStringField(TEXT("killCondition"), KillCondition);
    Context.Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured kill particles module: condition=%s"), *KillCondition));
    Context.SendSuccess(true, TEXT("Kill particles module configured."));
    return true;
}

static bool AddCameraOffsetModule(FActionContext& Context)
{
    UNiagaraSystem* System = nullptr;
    if (!AddModuleAndVerify(Context, TEXT("/Niagara/Modules/Update/Camera/CameraOffset.CameraOffset"), ENiagaraScriptUsage::ParticleUpdateScript, TEXT("CameraOffset"), System))
    {
        return true;
    }
    const double CameraOffset = GetJsonNumberField(Context.Payload, TEXT("cameraOffset"), 0.0);
    const bool bParameterAdded = AddOrSetFloatUserParameter(System, TEXT("MCP_CameraOffset"), static_cast<float>(CameraOffset));
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("CameraOffset"));
    Context.Result->SetNumberField(TEXT("cameraOffset"), CameraOffset);
    Context.Result->SetBoolField(TEXT("parameterAdded"), bParameterAdded);
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured camera offset module: offset=%.1f"), CameraOffset));
    Context.SendSuccess(true, TEXT("Camera offset module configured."));
    return true;
}

bool HandleDynamicsModuleAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("add_force_module")) return AddForceModule(Context);
    if (SubAction == TEXT("add_velocity_module")) return AddVelocityModule(Context);
    if (SubAction == TEXT("add_acceleration_module")) return AddAccelerationModule(Context);
    if (SubAction == TEXT("add_size_module")) return AddSizeModule(Context);
    if (SubAction == TEXT("add_color_module")) return AddColorModule(Context);
    if (SubAction == TEXT("add_collision_module")) return AddCollisionModule(Context);
    if (SubAction == TEXT("add_kill_particles_module")) return AddKillParticlesModule(Context);
    if (SubAction == TEXT("add_camera_offset_module")) return AddCameraOffsetModule(Context);
    return false;
}
}
#endif

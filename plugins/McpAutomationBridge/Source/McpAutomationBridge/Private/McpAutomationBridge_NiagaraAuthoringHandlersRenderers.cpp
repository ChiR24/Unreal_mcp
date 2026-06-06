#include "McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

#if WITH_EDITOR
namespace McpNiagaraAuthoringHandlers
{
struct FRendererTarget
{
    UNiagaraSystem* System = nullptr;
    MCP_NIAGARA_EMITTER_DATA_TYPE* EmitterData = nullptr;
    UNiagaraEmitter* Emitter = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FVersionedNiagaraEmitter VersionedEmitter;
#endif
};

static bool LoadRendererTarget(FActionContext& Context, FRendererTarget& Target)
{
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, Target.System, Handle))
    {
        return false;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Target.EmitterData = Handle->GetEmitterData();
    Target.VersionedEmitter = Handle->GetInstance();
    Target.Emitter = Target.VersionedEmitter.Emitter;
#else
    Target.EmitterData = Handle->GetInstance();
    Target.Emitter = Handle->GetInstance();
#endif
    return Target.EmitterData && Target.Emitter;
}

template <typename TRenderer>
static TRenderer* FindOrCreateRenderer(FRendererTarget& Target)
{
    for (UNiagaraRendererProperties* Renderer : Target.EmitterData->GetRenderers())
    {
        if (TRenderer* TypedRenderer = Cast<TRenderer>(Renderer))
        {
            return TypedRenderer;
        }
    }
    TRenderer* NewRenderer = NewObject<TRenderer>(Target.Emitter);
    if (!NewRenderer)
    {
        return nullptr;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Target.Emitter->AddRenderer(NewRenderer, Target.VersionedEmitter.Version);
#else
    Target.Emitter->AddRenderer(NewRenderer);
#endif
    return NewRenderer;
}

static bool AddSpriteRenderer(FActionContext& Context)
{
    FRendererTarget Target;
    if (!LoadRendererTarget(Context, Target))
    {
        return true;
    }
    UNiagaraSpriteRendererProperties* Renderer = FindOrCreateRenderer<UNiagaraSpriteRendererProperties>(Target);
    if (!Renderer)
    {
        Context.SendError(TEXT("Failed to create sprite renderer"), TEXT("CREATION_FAILED"));
        return true;
    }
    const FString MaterialPath = GetJsonStringField(Context.Payload, TEXT("materialPath"));
    if (!MaterialPath.IsEmpty())
    {
        if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath))
        {
            Renderer->Material = Material;
        }
    }
    MarkDirtyAndVerify(Context, Target.System);
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("SpriteRenderer"));
    Context.Result->SetStringField(TEXT("message"), TEXT("Configured sprite renderer module."));
    Context.SendSuccess(true, TEXT("Sprite renderer configured."));
    return true;
}

static bool AddMeshRenderer(FActionContext& Context)
{
    FRendererTarget Target;
    if (!LoadRendererTarget(Context, Target))
    {
        return true;
    }
    UNiagaraMeshRendererProperties* Renderer = FindOrCreateRenderer<UNiagaraMeshRendererProperties>(Target);
    if (!Renderer)
    {
        Context.SendError(TEXT("Failed to create mesh renderer"), TEXT("CREATION_FAILED"));
        return true;
    }
    const FString MeshPath = GetJsonStringField(Context.Payload, TEXT("meshPath"));
    if (!MeshPath.IsEmpty())
    {
        if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath))
        {
            FNiagaraMeshRendererMeshProperties MeshProps;
            MeshProps.Mesh = Mesh;
            Renderer->Meshes.Empty();
            Renderer->Meshes.Add(MeshProps);
        }
    }
    MarkDirtyAndVerify(Context, Target.System);
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("MeshRenderer"));
    Context.Result->SetStringField(TEXT("message"), TEXT("Configured mesh renderer module."));
    Context.SendSuccess(true, TEXT("Mesh renderer configured."));
    return true;
}

static bool AddRibbonRenderer(FActionContext& Context)
{
    FRendererTarget Target;
    if (!LoadRendererTarget(Context, Target))
    {
        return true;
    }
    UNiagaraRibbonRendererProperties* Renderer = FindOrCreateRenderer<UNiagaraRibbonRendererProperties>(Target);
    if (!Renderer)
    {
        Context.SendError(TEXT("Failed to create ribbon renderer"), TEXT("CREATION_FAILED"));
        return true;
    }
    const FString MaterialPath = GetJsonStringField(Context.Payload, TEXT("materialPath"));
    if (!MaterialPath.IsEmpty())
    {
        if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath))
        {
            Renderer->Material = Material;
        }
    }
    MarkDirtyAndVerify(Context, Target.System);
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("RibbonRenderer"));
    Context.Result->SetStringField(TEXT("message"), TEXT("Configured ribbon renderer module."));
    Context.SendSuccess(true, TEXT("Ribbon renderer configured."));
    return true;
}

static bool AddLightRenderer(FActionContext& Context)
{
    FRendererTarget Target;
    if (!LoadRendererTarget(Context, Target))
    {
        return true;
    }
    UNiagaraLightRendererProperties* Renderer = FindOrCreateRenderer<UNiagaraLightRendererProperties>(Target);
    if (!Renderer)
    {
        Context.SendError(TEXT("Failed to create light renderer"), TEXT("CREATION_FAILED"));
        return true;
    }
    Renderer->RadiusScale = static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("lightRadius"), 100.0));
    MarkDirtyAndVerify(Context, Target.System);
    Context.Result->SetStringField(TEXT("moduleName"), TEXT("LightRenderer"));
    Context.Result->SetStringField(TEXT("message"), TEXT("Configured light renderer module."));
    Context.SendSuccess(true, TEXT("Light renderer configured."));
    return true;
}

bool HandleRendererAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("add_sprite_renderer_module")) return AddSpriteRenderer(Context);
    if (SubAction == TEXT("add_mesh_renderer_module")) return AddMeshRenderer(Context);
    if (SubAction == TEXT("add_ribbon_renderer_module")) return AddRibbonRenderer(Context);
    if (SubAction == TEXT("add_light_renderer_module")) return AddLightRenderer(Context);
    return false;
}
}
#endif

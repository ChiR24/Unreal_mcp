#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

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

// Finds or adds the emitter's TRenderer, applies Configure to it, and replies naming it Label ("Sprite").
template <typename TRenderer, typename TConfigure>
static bool AddRenderer(FActionContext& Context, const TCHAR* Label, TConfigure Configure)
{
    FRendererTarget Target;
    if (!LoadRendererTarget(Context, Target))
    {
        return true;
    }
    TRenderer* Renderer = FindOrCreateRenderer<TRenderer>(Target);
    const FString Lower = FString(Label).ToLower();
    if (!Renderer)
    {
        Context.SendError(FString::Printf(TEXT("Failed to create %s renderer"), *Lower), TEXT("CREATION_FAILED"));
        return true;
    }
    Configure(*Renderer);
    MarkDirtyAndVerify(Context, Target.System);
    Context.Result->SetStringField(TEXT("moduleName"), FString::Printf(TEXT("%sRenderer"), Label));
    Context.Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Configured %s renderer module."), *Lower));
    Context.SendSuccess(true, FString::Printf(TEXT("%s renderer configured."), Label));
    return true;
}

bool HandleRendererAction(FActionContext& Context, const FString& SubAction)
{
    const auto SetMaterial = [&Context](auto& Renderer)
    {
        const FString Path = GetJsonStringField(Context.Payload, TEXT("materialPath"));
        if (UMaterialInterface* Material = Path.IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *Path))
        {
            Renderer.Material = Material;
        }
    };
    if (SubAction == TEXT("add_sprite_renderer_module"))
    {
        return AddRenderer<UNiagaraSpriteRendererProperties>(Context, TEXT("Sprite"), SetMaterial);
    }
    if (SubAction == TEXT("add_ribbon_renderer_module"))
    {
        return AddRenderer<UNiagaraRibbonRendererProperties>(Context, TEXT("Ribbon"), SetMaterial);
    }
    if (SubAction == TEXT("add_mesh_renderer_module"))
    {
        return AddRenderer<UNiagaraMeshRendererProperties>(Context, TEXT("Mesh"), [&Context](UNiagaraMeshRendererProperties& Renderer)
        {
            const FString Path = GetJsonStringField(Context.Payload, TEXT("meshPath"));
            if (UStaticMesh* Mesh = Path.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Path))
            {
                FNiagaraMeshRendererMeshProperties MeshProps;
                MeshProps.Mesh = Mesh;
                Renderer.Meshes.Empty();
                Renderer.Meshes.Add(MeshProps);
            }
        });
    }
    if (SubAction == TEXT("add_light_renderer_module"))
    {
        return AddRenderer<UNiagaraLightRendererProperties>(Context, TEXT("Light"), [&Context](UNiagaraLightRendererProperties& Renderer)
        {
            Renderer.RadiusScale = static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("lightRadius"), 100.0));
        });
    }
    return false;
}
}

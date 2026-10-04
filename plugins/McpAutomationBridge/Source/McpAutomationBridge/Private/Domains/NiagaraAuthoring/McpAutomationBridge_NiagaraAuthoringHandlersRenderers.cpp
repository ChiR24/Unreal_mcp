#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"
#include "Materials/Material.h"
#include "Safety/McpSafeOperationsAssetSave.h"

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
    TRenderer* NewRenderer = NewObject<TRenderer>(Target.Emitter, NAME_None, RF_Transactional);
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

// Loads the asset named by Field before any renderer changes; false (error sent) when it is named and does not load.
// A wrong path used to be skipped silently and the renderer reported configured.
template <typename TAsset>
static bool LoadRendererAsset(FActionContext& Context, const TCHAR* Field, TAsset*& OutAsset)
{
    const FString Path = GetJsonStringField(Context.Payload, Field);
    OutAsset = Path.IsEmpty() ? nullptr : LoadObject<TAsset>(nullptr, *Path);
    if (!Path.IsEmpty() && !OutAsset)
    {
        Context.SendError(FString::Printf(TEXT("%s '%s' is not a %s asset"), Field, *Path, *TAsset::StaticClass()->GetName()), TEXT("ASSET_NOT_FOUND"));
        return false;
    }
    return true;
}

// A material without its Niagara usage flag draws as the engine default material on the renderer. The editor sets
// a missing flag itself only outside Play In Editor and never saves it, so a material assigned during play, or one
// no viewport showed before a cook, rendered wrong: set it by reflection (the bitfields are deprecated for direct
// access in 5.8) as the details panel does, which recompiles, and save the project material.
static void EnsureNiagaraUsage(UMaterialInterface* Material, const TCHAR* FlagName)
{
    UMaterial* Base = Material ? Material->GetMaterial() : nullptr;
    FBoolProperty* Flag = Base ? FindFProperty<FBoolProperty>(UMaterial::StaticClass(), FlagName) : nullptr;
    if (!Flag || Flag->GetPropertyValue_InContainer(Base) || !Base->GetPathName().StartsWith(TEXT("/Game/")))
    {
        return;
    }
    Base->Modify();
    Flag->SetPropertyValue_InContainer(Base, true);
    Base->PostEditChange();
    McpSafeOperations::McpSafeAssetSave(Base);
}

bool HandleRendererAction(FActionContext& Context, const FString& SubAction)
{
    const bool bSprite = SubAction == TEXT("add_sprite_renderer_module");
    if (bSprite || SubAction == TEXT("add_ribbon_renderer_module"))
    {
        UMaterialInterface* Material = nullptr;
        if (!LoadRendererAsset(Context, TEXT("materialPath"), Material))
        {
            return true;
        }
        EnsureNiagaraUsage(Material, bSprite ? TEXT("bUsedWithNiagaraSprites") : TEXT("bUsedWithNiagaraRibbons"));
        const auto SetMaterial = [Material](auto& Renderer)
        {
            if (Material)
            {
                Renderer.Material = Material;
            }
        };
        return bSprite
            ? AddRenderer<UNiagaraSpriteRendererProperties>(Context, TEXT("Sprite"), SetMaterial)
            : AddRenderer<UNiagaraRibbonRendererProperties>(Context, TEXT("Ribbon"), SetMaterial);
    }
    if (SubAction == TEXT("add_mesh_renderer_module"))
    {
        UStaticMesh* Mesh = nullptr;
        if (!LoadRendererAsset(Context, TEXT("meshPath"), Mesh))
        {
            return true;
        }
        // Mesh particles draw the mesh's own materials, which need their mesh-particle flag the same way.
        for (const FStaticMaterial& Slot : Mesh ? Mesh->GetStaticMaterials() : TArray<FStaticMaterial>())
        {
            EnsureNiagaraUsage(Slot.MaterialInterface, TEXT("bUsedWithNiagaraMeshParticles"));
        }
        return AddRenderer<UNiagaraMeshRendererProperties>(Context, TEXT("Mesh"), [Mesh](UNiagaraMeshRendererProperties& Renderer)
        {
            if (Mesh)
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
            // RadiusScale multiplies each particle's light radius (engine default 1); the old
            // 100.0 default rewrote it on every call that did not send lightRadius.
            double RadiusScale = 1.0;
            if (Context.Payload->TryGetNumberField(TEXT("lightRadius"), RadiusScale))
            {
                Renderer.RadiusScale = static_cast<float>(RadiusScale);
            }
        });
    }
    return false;
}
}

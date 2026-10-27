#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"
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
static TRenderer* FindOrCreateRenderer(FRendererTarget& Target, bool& bOutCreated)
{
    bOutCreated = false;
    for (UNiagaraRendererProperties* Renderer : Target.EmitterData->GetRenderers())
    {
        if (TRenderer* TypedRenderer = Cast<TRenderer>(Renderer))
        {
            return TypedRenderer;
        }
    }
    bOutCreated = true;
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
    bool bCreated = false;
    TRenderer* Renderer = FindOrCreateRenderer<TRenderer>(Target, bCreated);
    const FString Lower = FString(Label).ToLower();
    if (!Renderer)
    {
        Context.SendError(FString::Printf(TEXT("Failed to create %s renderer"), *Lower), TEXT("CREATION_FAILED"));
        return true;
    }
    Configure(*Renderer);
    MarkDirtyAndVerify(Context, Target.System);
    // A second call configured the emitter's existing renderer while the reply read the same as adding one.
    Context.Result->SetStringField(TEXT("moduleName"), FString::Printf(TEXT("%sRenderer"), Label));
    Context.Result->SetStringField(TEXT("rendererPath"), Renderer->GetPathName());
    Context.Result->SetBoolField(TEXT("rendererCreated"), bCreated);
    Context.SendSuccess(true, bCreated ? FString::Printf(TEXT("%s renderer added."), Label)
                                       : FString::Printf(TEXT("The emitter's %s renderer was configured; it already had one."), *Lower));
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
        const auto SetMaterial = [Material, &Context](auto& Renderer)
        {
            if (Material)
            {
                Renderer.Material = Material;
            }
            // Read back: without materialPath the renderer keeps what it had, the engine default on a new one.
            Context.Result->SetStringField(TEXT("materialPath"), Renderer.Material ? Renderer.Material->GetPathName() : FString());
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
        // Without a mesh the renderer was added with an empty mesh list, drawing nothing, and answered configured.
        if (!Mesh)
        {
            Context.SendError(TEXT("meshPath is required: a mesh renderer with no mesh draws nothing. Nothing was changed."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        // Mesh particles draw the mesh's own materials, which need their mesh-particle flag the same way.
        for (const FStaticMaterial& Slot : Mesh ? Mesh->GetStaticMaterials() : TArray<FStaticMaterial>())
        {
            EnsureNiagaraUsage(Slot.MaterialInterface, TEXT("bUsedWithNiagaraMeshParticles"));
        }
        return AddRenderer<UNiagaraMeshRendererProperties>(Context, TEXT("Mesh"), [Mesh, &Context](UNiagaraMeshRendererProperties& Renderer)
        {
            FNiagaraMeshRendererMeshProperties MeshProps;
            MeshProps.Mesh = Mesh;
            Renderer.Meshes.Empty();
            Renderer.Meshes.Add(MeshProps);
            Context.Result->SetStringField(TEXT("meshPath"), Mesh->GetPathName());
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
            Context.Result->SetNumberField(TEXT("lightRadius"), Renderer.RadiusScale);
        });
    }
    return false;
}
}

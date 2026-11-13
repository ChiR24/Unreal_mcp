#include "Domains/PCG/McpPCGPrivate.h"
#include "Engine/Texture.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
static bool ApplySpawnActorTemplateClass(UPCGSettings* Settings, const FString& ClassName, FString& OutError)
{
    if (ClassName.IsEmpty())
    {
        return true;
    }

    UPCGSpawnActorSettings* SpawnActorSettings = Cast<UPCGSpawnActorSettings>(Settings);
    if (!SpawnActorSettings)
    {
        OutError = FString::Printf(TEXT("PCG settings '%s' are not actor spawner settings."), Settings ? *Settings->GetClass()->GetName() : TEXT("<null>"));
        return false;
    }

    // TemplateActorClass is a class property on every version that has PCG: its MetaClass bounds the class.
    FClassProperty* ClassProperty = CastField<FClassProperty>(SpawnActorSettings->GetClass()->FindPropertyByName(FName(TEXT("TemplateActorClass"))));
    if (!ClassProperty)
    {
        OutError = FString::Printf(TEXT("Class property 'TemplateActorClass' was not found on '%s'."), *SpawnActorSettings->GetClass()->GetName());
        return false;
    }
    UClass* ActorClass = ResolveClassByName(ClassName);
    if (!ActorClass)
    {
        OutError = FString::Printf(TEXT("Could not resolve class '%s'."), *ClassName);
        return false;
    }
    if (ClassProperty->MetaClass && !ActorClass->IsChildOf(ClassProperty->MetaClass))
    {
        OutError = FString::Printf(TEXT("Class '%s' is not assignable to '%s'."), *ActorClass->GetName(), *ClassProperty->MetaClass->GetName());
        return false;
    }

    SpawnActorSettings->Modify();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    SpawnActorSettings->SetTemplateActorClass(TSubclassOf<AActor>(ActorClass));
#else
    ClassProperty->SetPropertyValue_InContainer(SpawnActorSettings, ActorClass);
#endif
    return true;
}

static bool ApplyStaticMeshSpawnerMeshPath(UPCGSettings* Settings, const FString& MeshPath, FString& OutError)
{
    if (MeshPath.IsEmpty())
    {
        return true;
    }

    UPCGStaticMeshSpawnerSettings* SpawnerSettings = Cast<UPCGStaticMeshSpawnerSettings>(Settings);
    if (!SpawnerSettings)
    {
        OutError = FString::Printf(TEXT("PCG settings '%s' are not static mesh spawner settings."), Settings ? *Settings->GetClass()->GetName() : TEXT("<null>"));
        return false;
    }

    FString ResolvedMeshPath;
    UStaticMesh* StaticMesh = LoadPCGAsset<UStaticMesh>(MeshPath, TEXT("static mesh"), ResolvedMeshPath, OutError);
    if (!StaticMesh)
    {
        return false;
    }

    SpawnerSettings->Modify();
    // UE 5.3 renamed the spawner's selector instance from MeshSelectorInstance to MeshSelectorParameters.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
    TObjectPtr<UPCGMeshSelectorBase>& Selector = SpawnerSettings->MeshSelectorInstance;
#else
    TObjectPtr<UPCGMeshSelectorBase>& Selector = SpawnerSettings->MeshSelectorParameters;
#endif
    if (!Selector || !Selector->IsA<UPCGMeshSelectorWeighted>())
    {
        SpawnerSettings->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
    }

    UPCGMeshSelectorWeighted* WeightedSelector = Cast<UPCGMeshSelectorWeighted>(Selector);
    if (!WeightedSelector)
    {
        OutError = TEXT("Could not create weighted mesh selector for PCG static mesh spawner.");
        return false;
    }

    WeightedSelector->Modify();
    WeightedSelector->MeshEntries.Reset();
    FPCGMeshSelectorWeightedEntry& Entry = WeightedSelector->MeshEntries.AddDefaulted_GetRef();
    Entry.Descriptor.StaticMesh = StaticMesh;
    Entry.Weight = 1;
    return true;
}

bool ApplyPCGConvenienceSettings(const FString& SubAction, UPCGSettings* Settings, const TSharedPtr<FJsonObject>& Payload, FString& OutError, int32& OutAppliedCount)
{
    OutAppliedCount = 0;
    if (!Settings || !Payload.IsValid())
    {
        return true;
    }

    if (SubAction == TEXT("add_texture_data_node") || Payload->HasField(TEXT("texturePath")))
    {
        const FString TexturePath = GetJsonStringField(Payload, TEXT("texturePath"));
        if (!TexturePath.IsEmpty())
        {
            // Loaded up front through the shared normalizer instead of storing an unresolvable path.
            FString ResolvedTexturePath;
            if (!LoadPCGAsset<UTexture>(TexturePath, TEXT("texture"), ResolvedTexturePath, OutError))
            {
                return false;
            }
            if (!ApplyStringSetting(Settings, TEXT("Texture"), ResolvedTexturePath, OutError))
            {
                return false;
            }
            ++OutAppliedCount;
        }
    }
    if (SubAction == TEXT("add_mesh_sampler") || (Payload->HasField(TEXT("meshPath")) && Settings->GetClass()->FindPropertyByName(FName(TEXT("Mesh")))))
    {
        const FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
        if (!MeshPath.IsEmpty())
        {
            // Loaded up front: a bare or wrong path used to be stored as text
            // and only LogPCG complained at generation time.
            FString ResolvedSamplerMeshPath;
            if (!LoadPCGAsset<UStaticMesh>(MeshPath, TEXT("static mesh"), ResolvedSamplerMeshPath, OutError))
            {
                return false;
            }
            if (!ApplyStringSetting(Settings, TEXT("Mesh"), ResolvedSamplerMeshPath, OutError))
            {
                return false;
            }
            ++OutAppliedCount;
        }
    }
    if (SubAction == TEXT("add_static_mesh_spawner") || (Payload->HasField(TEXT("meshPath")) && Settings->IsA<UPCGStaticMeshSpawnerSettings>()))
    {
        const FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
        if (!MeshPath.IsEmpty())
        {
            if (!ApplyStaticMeshSpawnerMeshPath(Settings, MeshPath, OutError))
            {
                return false;
            }
            ++OutAppliedCount;
        }
    }
    if (SubAction == TEXT("add_actor_spawner") || Payload->HasField(TEXT("actorClass")) || (Payload->HasField(TEXT("classPath")) && Settings->IsA<UPCGSpawnActorSettings>()))
    {
        const FString ActorClass = McpGetFirstStringField(Payload, {TEXT("actorClass"), TEXT("classPath")});
        if (!ActorClass.IsEmpty())
        {
            if (!ApplySpawnActorTemplateClass(Settings, ActorClass, OutError))
            {
                return false;
            }
            ++OutAppliedCount;
        }
    }

    return true;
}
}
#endif

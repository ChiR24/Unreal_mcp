#include "McpAutomationBridge_PCGHandlersPrivate.h"

#if WITH_EDITOR && MCP_HAS_PCG
namespace McpPCGHandlers
{
bool ApplySpawnActorTemplateClass(UPCGSettings* Settings, const FString& ClassName, FString& OutError)
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

    UClass* ActorClass = nullptr;
    if (!ResolveClassForProperty(SpawnActorSettings, TEXT("TemplateActorClass"), ClassName, ActorClass, OutError))
    {
        return false;
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    TSubclassOf<AActor> ActorSubclass = ActorClass;
    SpawnActorSettings->Modify();
    SpawnActorSettings->SetTemplateActorClass(ActorSubclass);
#else
    FClassProperty* ClassProperty = CastField<FClassProperty>(SpawnActorSettings->GetClass()->FindPropertyByName(FName(TEXT("TemplateActorClass"))));
    if (!ClassProperty)
    {
        OutError = FString::Printf(TEXT("Class property 'TemplateActorClass' was not found on '%s'."), *SpawnActorSettings->GetClass()->GetName());
        return false;
    }

    SpawnActorSettings->Modify();
    ClassProperty->SetPropertyValue_InContainer(SpawnActorSettings, ActorClass);
#endif
    return true;
}

UStaticMesh* LoadPCGStaticMesh(const FString& RawMeshPath, FString& OutResolvedPath, FString& OutError)
{
    FNormalizedAssetPath Normalized = NormalizeAssetPath(RawMeshPath);
    if (!Normalized.bIsValid)
    {
        OutError = Normalized.ErrorMessage;
        return nullptr;
    }

    OutResolvedPath = Normalized.Path;
    UObject* Loaded = UEditorAssetLibrary::LoadAsset(OutResolvedPath);
    if (!Loaded)
    {
        Loaded = StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *ToObjectPath(OutResolvedPath));
    }

    UStaticMesh* StaticMesh = Cast<UStaticMesh>(Loaded);
    if (!StaticMesh)
    {
        OutError = FString::Printf(TEXT("Could not load static mesh at '%s'."), *OutResolvedPath);
        return nullptr;
    }

    return StaticMesh;
}

bool ApplyStaticMeshSpawnerMeshPath(UPCGSettings* Settings, const FString& MeshPath, FString& OutError)
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
    UStaticMesh* StaticMesh = LoadPCGStaticMesh(MeshPath, ResolvedMeshPath, OutError);
    if (!StaticMesh)
    {
        return false;
    }

    SpawnerSettings->Modify();
    if (!SpawnerSettings->MeshSelectorParameters || !SpawnerSettings->MeshSelectorParameters->IsA<UPCGMeshSelectorWeighted>())
    {
        SpawnerSettings->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
    }

    UPCGMeshSelectorWeighted* WeightedSelector = Cast<UPCGMeshSelectorWeighted>(SpawnerSettings->MeshSelectorParameters);
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
            if (!ApplyStringSetting(Settings, TEXT("Texture"), TexturePath, OutError))
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
            if (!ApplyStringSetting(Settings, TEXT("Mesh"), MeshPath, OutError))
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
        const FString ActorClass = GetFirstStringField(Payload, {TEXT("actorClass"), TEXT("classPath")});
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

#include "Domains/PCG/McpPCGPrivate.h"
#include "Engine/Texture.h"
#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Elements/PCGUserParameterGet.h"
#endif

#if MCP_HAS_PCG
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
    UStaticMesh* StaticMesh = LoadPCGAsset<UStaticMesh>(MeshPath, TEXT("static mesh"), ResolvedMeshPath, OutError);
    if (!StaticMesh)
    {
        return false;
    }

    SpawnerSettings->Modify();
    if (!SpawnerSettings->MeshSelectorParameters || !SpawnerSettings->MeshSelectorParameters->IsA<UPCGMeshSelectorWeighted>())
    {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
        auto Fields = MakeShared<FJsonObject>();
        Fields->SetStringField(TEXT("MeshSelectorType"), TEXT("/Script/PCG.PCGMeshSelectorWeighted"));
        int32 Applied = 0;
        if (!ApplySettingsObject(SpawnerSettings, Fields, OutError, Applied)) return false;
#else
        SpawnerSettings->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
#endif
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

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
    if (Payload->HasField(TEXT("parameterName")))
    {
        UPCGUserParameterGetSettings* Getter = Cast<UPCGUserParameterGetSettings>(Settings);
        FString GraphPath;
        UPCGGraph* Graph = LoadPCGAsset<UPCGGraph>(GetJsonStringField(Payload, TEXT("graphPath")), TEXT("parameter graph"), GraphPath, OutError);
        const FInstancedPropertyBag* Bag = Graph ? Graph->GetUserParametersStruct() : nullptr;
        const FPropertyBagPropertyDesc* Parameter = Bag ? Bag->FindPropertyDescByName(FName(*GetJsonStringField(Payload, TEXT("parameterName")))) : nullptr;
        if (!Getter || !Parameter)
        {
            OutError = TEXT("parameterName must identify a current parameter and requires PCGUserParameterGetSettings.");
            return false;
        }
        Getter->PropertyGuid = Parameter->ID;
        Getter->PropertyName = Parameter->Name;
        ++OutAppliedCount;
    }
#endif
    if (SubAction == TEXT("add_texture_data_node") || Payload->HasField(TEXT("texturePath")))
    {
        const FString TexturePath = GetJsonStringField(Payload, TEXT("texturePath"));
        if (!TexturePath.IsEmpty())
        {
            // Validate the texture up front (dogfood #184) instead of storing an unresolvable path.
            if (!LoadObject<UTexture>(nullptr, *TexturePath))
            {
                OutError = FString::Printf(TEXT("texturePath not found: %s"), *TexturePath);
                return false;
            }
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
            // Validate the mesh up front (dogfood #184): a bare or wrong path used to be stored as text
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
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
            const TSharedPtr<FJsonObject>* Fields = nullptr;
            if (Payload->TryGetObjectField(TEXT("settings"), Fields) && Fields && Fields->IsValid())
            {
                for (const auto& Field : (*Fields)->Values)
                {
                    TArray<FString> Segments;
                    const FString FieldName(Field.Key.Len(), *Field.Key);
                    FieldName.ParseIntoArray(Segments, TEXT("."), true);
                    const auto* Spawner = Cast<UPCGStaticMeshSpawnerSettings>(Settings);
                    if (Segments.Num() == 1 && Segments[0].Equals(TEXT("MeshSelectorType"), ESearchCase::IgnoreCase) &&
                        Spawner && Spawner->MeshSelectorType != UPCGMeshSelectorWeighted::StaticClass())
                    {
                        OutError = TEXT("meshPath resets a weighted selector; it cannot be combined with an explicit different selector type.");
                        return false;
                    }
                }
            }
#endif
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

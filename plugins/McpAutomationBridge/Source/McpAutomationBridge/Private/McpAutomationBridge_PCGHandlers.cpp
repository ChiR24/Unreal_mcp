#include "McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "MCP/McpConsolidatedActionRouting.h"

#include <initializer_list>

#ifndef MCP_HAS_PCG
#define MCP_HAS_PCG 0
#endif

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EditorAssetLibrary.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#endif

#if WITH_EDITOR && MCP_HAS_PCG
#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "PCGSubgraph.h"
#include "PCGWorldActor.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGSpawnActor.h"
#include "Engine/StaticMesh.h"
#include "Helpers/PCGHelpers.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#endif

#if WITH_EDITOR && MCP_HAS_PCG
namespace
{
FString NormalizePCGSubAction(const TSharedPtr<FJsonObject>& Payload)
{
    return McpConsolidatedActions::GetPayloadSubAction(Payload);
}

FString GetFirstStringField(const TSharedPtr<FJsonObject>& Payload, std::initializer_list<const TCHAR*> Fields)
{
    if (!Payload.IsValid())
    {
        return FString();
    }

    for (const TCHAR* Field : Fields)
    {
        FString Value;
        if (Payload->TryGetStringField(Field, Value) && !Value.IsEmpty())
        {
            return Value;
        }
    }

    return FString();
}

struct FPCGSettingsAlias
{
    const TCHAR* Alias;
    const TCHAR* SettingsClass;
};

const FPCGSettingsAlias* FindPCGSettingsAlias(const FString& RawAlias)
{
    static const FPCGSettingsAlias Aliases[] = {
        {TEXT("add_landscape_data_node"), TEXT("PCGGetLandscapeSettings")},
        {TEXT("landscape_data"), TEXT("PCGGetLandscapeSettings")},
        {TEXT("add_spline_data_node"), TEXT("PCGGetSplineSettings")},
        {TEXT("spline_data"), TEXT("PCGGetSplineSettings")},
        {TEXT("add_volume_data_node"), TEXT("PCGGetVolumeSettings")},
        {TEXT("volume_data"), TEXT("PCGGetVolumeSettings")},
        {TEXT("add_actor_data_node"), TEXT("PCGDataFromActorSettings")},
        {TEXT("actor_data"), TEXT("PCGDataFromActorSettings")},
        {TEXT("add_texture_data_node"), TEXT("PCGTextureSamplerSettings")},
        {TEXT("texture_data"), TEXT("PCGTextureSamplerSettings")},
        {TEXT("add_surface_sampler"), TEXT("PCGSurfaceSamplerSettings")},
        {TEXT("surface_sampler"), TEXT("PCGSurfaceSamplerSettings")},
        {TEXT("add_mesh_sampler"), TEXT("PCGPointFromMeshSettings")},
        {TEXT("mesh_sampler"), TEXT("PCGPointFromMeshSettings")},
        {TEXT("add_spline_sampler"), TEXT("PCGSplineSamplerSettings")},
        {TEXT("spline_sampler"), TEXT("PCGSplineSamplerSettings")},
        {TEXT("add_volume_sampler"), TEXT("PCGVolumeSamplerSettings")},
        {TEXT("volume_sampler"), TEXT("PCGVolumeSamplerSettings")},
        {TEXT("add_bounds_modifier"), TEXT("PCGBoundsModifierSettings")},
        {TEXT("bounds_modifier"), TEXT("PCGBoundsModifierSettings")},
        {TEXT("add_density_filter"), TEXT("PCGDensityFilterSettings")},
        {TEXT("density_filter"), TEXT("PCGDensityFilterSettings")},
        {TEXT("add_height_filter"), TEXT("PCGAttributeFilteringRangeSettings")},
        {TEXT("height_filter"), TEXT("PCGAttributeFilteringRangeSettings")},
        {TEXT("add_slope_filter"), TEXT("PCGNormalToDensitySettings")},
        {TEXT("slope_filter"), TEXT("PCGNormalToDensitySettings")},
        {TEXT("add_distance_filter"), TEXT("PCGDistanceSettings")},
        {TEXT("distance_filter"), TEXT("PCGDistanceSettings")},
        {TEXT("add_bounds_filter"), TEXT("PCGCullPointsOutsideActorBoundsSettings")},
        {TEXT("bounds_filter"), TEXT("PCGCullPointsOutsideActorBoundsSettings")},
        {TEXT("add_self_pruning"), TEXT("PCGSelfPruningSettings")},
        {TEXT("self_pruning"), TEXT("PCGSelfPruningSettings")},
        {TEXT("add_transform_points"), TEXT("PCGTransformPointsSettings")},
        {TEXT("transform_points"), TEXT("PCGTransformPointsSettings")},
        {TEXT("add_project_to_surface"), TEXT("PCGProjectionSettings")},
        {TEXT("project_to_surface"), TEXT("PCGProjectionSettings")},
        {TEXT("add_copy_points"), TEXT("PCGCopyPointsSettings")},
        {TEXT("copy_points"), TEXT("PCGCopyPointsSettings")},
        {TEXT("add_merge_points"), TEXT("PCGMergeSettings")},
        {TEXT("merge_points"), TEXT("PCGMergeSettings")},
        {TEXT("add_static_mesh_spawner"), TEXT("PCGStaticMeshSpawnerSettings")},
        {TEXT("static_mesh_spawner"), TEXT("PCGStaticMeshSpawnerSettings")},
        {TEXT("add_actor_spawner"), TEXT("PCGSpawnActorSettings")},
        {TEXT("actor_spawner"), TEXT("PCGSpawnActorSettings")},
        {TEXT("add_spline_spawner"), TEXT("PCGSpawnSplineSettings")},
        {TEXT("spline_spawner"), TEXT("PCGSpawnSplineSettings")}
    };

    FString Normalized = RawAlias.TrimStartAndEnd().ToLower();
    Normalized.ReplaceInline(TEXT("-"), TEXT("_"));
    Normalized.ReplaceInline(TEXT(" "), TEXT("_"));
    for (const FPCGSettingsAlias& Alias : Aliases)
    {
        if (Normalized.Equals(Alias.Alias, ESearchCase::IgnoreCase))
        {
            return &Alias;
        }
    }
    return nullptr;
}

bool IsPCGNodeCreationAction(const FString& SubAction)
{
    return SubAction.StartsWith(TEXT("add_"), ESearchCase::IgnoreCase) && FindPCGSettingsAlias(SubAction) != nullptr;
}

bool TryGetPCGAssetPath(const TSharedPtr<FJsonObject>& Payload, std::initializer_list<const TCHAR*> DirectFields, FString& OutPath, FString& OutError)
{
    OutPath = GetFirstStringField(Payload, DirectFields);
    if (OutPath.IsEmpty())
    {
        const FString Directory = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/PCG"));
        const FString Name = GetJsonStringField(Payload, TEXT("name"));
        if (!Name.IsEmpty())
        {
            OutPath = Directory / Name;
        }
    }

    if (OutPath.IsEmpty())
    {
        OutError = TEXT("Missing PCG asset path. Provide graphPath, subgraphPath, assetPath, or path + name.");
        return false;
    }

    FNormalizedAssetPath Normalized = NormalizeAssetPath(OutPath);
    if (!Normalized.bIsValid)
    {
        OutError = Normalized.ErrorMessage;
        return false;
    }

    OutPath = Normalized.Path;
    return true;
}

FString ToObjectPath(const FString& PackagePath)
{
    return FString::Printf(TEXT("%s.%s"), *PackagePath, *FPackageName::GetShortName(PackagePath));
}

UPCGGraph* LoadPCGGraph(const FString& RawPath, FString& OutPath, FString& OutError)
{
    FNormalizedAssetPath Normalized = NormalizeAssetPath(RawPath);
    if (!Normalized.bIsValid)
    {
        OutError = Normalized.ErrorMessage;
        return nullptr;
    }

    OutPath = Normalized.Path;
    UObject* Loaded = UEditorAssetLibrary::LoadAsset(OutPath);
    if (!Loaded)
    {
        Loaded = StaticLoadObject(UPCGGraph::StaticClass(), nullptr, *ToObjectPath(OutPath));
    }

    UPCGGraph* Graph = Cast<UPCGGraph>(Loaded);
    if (!Graph)
    {
        OutError = FString::Printf(TEXT("Could not load PCG graph at '%s'."), *OutPath);
    }

    return Graph;
}

UPCGGraph* CreateOrReusePCGGraph(const FString& GraphPath, bool bOverwrite, bool bSave, bool& bOutCreated, bool& bOutSaved, FString& OutError)
{
    bOutCreated = false;
    bOutSaved = false;

    if (UEditorAssetLibrary::DoesAssetExist(GraphPath))
    {
        FString LoadedPath;
        UPCGGraph* Existing = LoadPCGGraph(GraphPath, LoadedPath, OutError);
        if (!Existing)
        {
            return nullptr;
        }
        if (!bOverwrite)
        {
            OutError = FString::Printf(TEXT("PCG graph already exists at '%s'."), *GraphPath);
            return nullptr;
        }
        return Existing;
    }

    const FString AssetName = FPackageName::GetShortName(GraphPath);
    UPackage* Package = CreatePackage(*GraphPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package '%s'."), *GraphPath);
        return nullptr;
    }

    UPCGGraph* Graph = NewObject<UPCGGraph>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
    if (!Graph)
    {
        OutError = FString::Printf(TEXT("Failed to create PCG graph '%s'."), *GraphPath);
        return nullptr;
    }

    Graph->MarkPackageDirty();
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Graph);
    bOutCreated = true;

    if (bSave)
    {
        bOutSaved = McpSafeAssetSave(Graph);
        if (!bOutSaved)
        {
            OutError = FString::Printf(TEXT("Created PCG graph '%s' but failed to save it."), *GraphPath);
            return nullptr;
        }
    }

    return Graph;
}

TSharedPtr<FJsonObject> BuildGraphResult(UPCGGraph* Graph, const FString& GraphPath, bool bCreated, bool bSaved)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    Result->SetStringField(TEXT("assetPath"), GraphPath);
    Result->SetStringField(TEXT("name"), FPackageName::GetShortName(GraphPath));
    Result->SetBoolField(TEXT("created"), bCreated);
    Result->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Result, Graph);
    return Result;
}

FString GetNodeTitleString(UPCGNode* Node)
{
    if (!Node)
    {
        return FString();
    }
    if (Node->NodeTitle != NAME_None)
    {
        return Node->NodeTitle.ToString();
    }
    return Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString();
}

TSharedPtr<FJsonObject> BuildNodeResult(UPCGGraph* Graph, UPCGNode* Node, const FString& GraphPath)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    if (Node)
    {
        Result->SetStringField(TEXT("nodeId"), Node->GetName());
        Result->SetStringField(TEXT("nodeName"), Node->GetName());
        Result->SetStringField(TEXT("title"), GetNodeTitleString(Node));
        if (UPCGSettings* Settings = Node->GetSettings())
        {
            Result->SetStringField(TEXT("nodeType"), Settings->GetClass()->GetName());
        }
        McpHandlerUtils::AddVerification(Result, Node);
    }
    else
    {
        McpHandlerUtils::AddVerification(Result, Graph);
    }
    return Result;
}

UPCGNode* FindPCGNode(UPCGGraph* Graph, const FString& NodeId)
{
    if (!Graph || NodeId.IsEmpty())
    {
        return nullptr;
    }

    const FString Needle = NodeId.TrimStartAndEnd();
    if (Needle.Equals(TEXT("input"), ESearchCase::IgnoreCase) || Needle.Equals(TEXT("input_node"), ESearchCase::IgnoreCase))
    {
        return Graph->GetInputNode();
    }
    if (Needle.Equals(TEXT("output"), ESearchCase::IgnoreCase) || Needle.Equals(TEXT("output_node"), ESearchCase::IgnoreCase))
    {
        return Graph->GetOutputNode();
    }
    if (Needle.IsNumeric())
    {
        const int32 Index = FCString::Atoi(*Needle);
        const TArray<UPCGNode*>& Nodes = Graph->GetNodes();
        if (Index >= 0 && Index < Nodes.Num())
        {
            return Nodes[Index];
        }
    }

    const TArray<UPCGNode*>& Nodes = Graph->GetNodes();
    for (UPCGNode* Node : Nodes)
    {
        if (!Node)
        {
            continue;
        }
        if (Node->GetName().Equals(Needle, ESearchCase::IgnoreCase) || Node->GetPathName().Equals(Needle, ESearchCase::IgnoreCase))
        {
            return Node;
        }
        if (Node->NodeTitle != NAME_None && Node->NodeTitle.ToString().Equals(Needle, ESearchCase::IgnoreCase))
        {
            return Node;
        }
        if (Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString().Equals(Needle, ESearchCase::IgnoreCase) ||
            Node->GetNodeTitle(EPCGNodeTitleType::FullTitle).ToString().Equals(Needle, ESearchCase::IgnoreCase))
        {
            return Node;
        }
    }

    return nullptr;
}

FString DescribePCGPinLabels(const TArray<TObjectPtr<UPCGPin>>& Pins)
{
    TArray<FString> Labels;
    for (const TObjectPtr<UPCGPin>& Pin : Pins)
    {
        if (Pin)
        {
            Labels.Add(Pin->Properties.Label.ToString());
        }
    }

    return Labels.IsEmpty() ? TEXT("<none>") : FString::Join(Labels, TEXT(", "));
}

UPCGPin* GetPCGPinByLabel(UPCGNode* Node, bool bOutputPin, const FName& Label)
{
    if (!Node)
    {
        return nullptr;
    }

    return bOutputPin ? Node->GetOutputPin(Label) : Node->GetInputPin(Label);
}

bool TryResolvePCGPinLabel(UPCGNode* Node, bool bOutputPin, const FString& RequestedPinLabel, FName& OutPinLabel, FString& OutError)
{
    if (!Node)
    {
        OutError = TEXT("PCG node is invalid.");
        return false;
    }

    const TCHAR* PinKind = bOutputPin ? TEXT("output") : TEXT("input");
    const TArray<TObjectPtr<UPCGPin>>& Pins = bOutputPin ? Node->GetOutputPins() : Node->GetInputPins();
    if (!RequestedPinLabel.IsEmpty())
    {
        const FName RequestedPin(*RequestedPinLabel);
        if (GetPCGPinByLabel(Node, bOutputPin, RequestedPin))
        {
            OutPinLabel = RequestedPin;
            return true;
        }

        OutError = FString::Printf(TEXT("PCG node '%s' has no %s pin '%s'. Available %s pins: %s."),
            *Node->GetName(), PinKind, *RequestedPinLabel, PinKind, *DescribePCGPinLabels(Pins));
        return false;
    }

    const FName PreferredPin = bOutputPin ? PCGPinConstants::DefaultOutputLabel : PCGPinConstants::DefaultInputLabel;
    if (GetPCGPinByLabel(Node, bOutputPin, PreferredPin))
    {
        OutPinLabel = PreferredPin;
        return true;
    }

    for (const TObjectPtr<UPCGPin>& Pin : Pins)
    {
        if (Pin)
        {
            OutPinLabel = Pin->Properties.Label;
            return true;
        }
    }

    OutError = FString::Printf(TEXT("PCG node '%s' has no %s pins."), *Node->GetName(), PinKind);
    return false;
}

bool HasPCGEdge(const UPCGPin* SourcePin, const UPCGPin* TargetPin)
{
    if (!SourcePin || !TargetPin)
    {
        return false;
    }

    for (const TObjectPtr<UPCGEdge>& Edge : SourcePin->Edges)
    {
        if (Edge && Edge->IsValid() && Edge->GetOtherPin(SourcePin) == TargetPin)
        {
            return true;
        }
    }

    return false;
}

UClass* ResolvePCGSettingsClass(const FString& RawClassName)
{
    if (RawClassName.IsEmpty())
    {
        return nullptr;
    }

    if (RawClassName.Equals(TEXT("subgraph"), ESearchCase::IgnoreCase) || RawClassName.Equals(TEXT("pcg_subgraph"), ESearchCase::IgnoreCase))
    {
        return UPCGSubgraphSettings::StaticClass();
    }

    TArray<FString> Candidates;
    const FString Trimmed = RawClassName.TrimStartAndEnd();
    Candidates.Add(Trimmed);

    if (const FPCGSettingsAlias* Alias = FindPCGSettingsAlias(Trimmed))
    {
        Candidates.Add(Alias->SettingsClass);
    }

    FString ShortName = Trimmed;
    int32 DotIndex = INDEX_NONE;
    if (ShortName.FindLastChar(TEXT('.'), DotIndex))
    {
        ShortName = ShortName.RightChop(DotIndex + 1);
    }
    if (ShortName.StartsWith(TEXT("U")) && ShortName.StartsWith(TEXT("UPCG")))
    {
        ShortName = ShortName.RightChop(1);
    }

    Candidates.Add(ShortName);
    if (!ShortName.EndsWith(TEXT("Settings")))
    {
        Candidates.Add(ShortName + TEXT("Settings"));
    }
    if (!ShortName.StartsWith(TEXT("PCG")))
    {
        Candidates.Add(TEXT("PCG") + ShortName);
        Candidates.Add(TEXT("PCG") + ShortName + TEXT("Settings"));
    }

    for (const FString& Candidate : Candidates)
    {
        if (UClass* Class = ResolveClassByName(Candidate))
        {
            if (Class->IsChildOf(UPCGSettings::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
            {
                return Class;
            }
        }

        static const TCHAR* ScriptModules[] = {TEXT("PCG"), TEXT("PCGGeometryScriptInterop")};
        for (const TCHAR* ScriptModule : ScriptModules)
        {
            const FString ScriptPath = FString::Printf(TEXT("/Script/%s.%s"), ScriptModule, *Candidate);
            if (UClass* Class = FindObject<UClass>(nullptr, *ScriptPath))
            {
                if (Class->IsChildOf(UPCGSettings::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
                {
                    return Class;
                }
            }
            if (UClass* Class = LoadObject<UClass>(nullptr, *ScriptPath))
            {
                if (Class->IsChildOf(UPCGSettings::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
                {
                    return Class;
                }
            }
        }
    }

    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        if (Class && Class->IsChildOf(UPCGSettings::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
        {
            for (const FString& Candidate : Candidates)
            {
                if (Class->GetName().Equals(Candidate, ESearchCase::IgnoreCase))
                {
                    return Class;
                }
            }
        }
    }

    return nullptr;
}

bool ApplySettingsObject(UPCGSettings* Settings, const TSharedPtr<FJsonObject>& SettingsObject, FString& OutError, int32& OutAppliedCount)
{
    OutAppliedCount = 0;
    if (!Settings || !SettingsObject.IsValid())
    {
        return true;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : SettingsObject->Values)
    {
        FProperty* Property = Settings->GetClass()->FindPropertyByName(FName(*Pair.Key));
        if (!Property)
        {
            OutError = FString::Printf(TEXT("PCG settings property '%s' was not found on '%s'."), *Pair.Key, *Settings->GetClass()->GetName());
            return false;
        }

        FString ApplyError;
        if (!ApplyJsonValueToProperty(Settings, Property, Pair.Value, ApplyError))
        {
            OutError = FString::Printf(TEXT("Failed to apply PCG settings property '%s': %s"), *Pair.Key, *ApplyError);
            return false;
        }
        ++OutAppliedCount;
    }

    return true;
}

bool ApplyStringSetting(UPCGSettings* Settings, const TCHAR* PropertyName, const FString& Value, FString& OutError)
{
    if (!Settings || Value.IsEmpty())
    {
        return true;
    }

    FProperty* Property = Settings->GetClass()->FindPropertyByName(FName(PropertyName));
    if (!Property)
    {
        OutError = FString::Printf(TEXT("PCG settings property '%s' was not found on '%s'."), PropertyName, *Settings->GetClass()->GetName());
        return false;
    }

    return ApplyJsonValueToProperty(Settings, Property, MakeShared<FJsonValueString>(Value), OutError);
}

bool ResolveClassForProperty(UObject* Target, const TCHAR* PropertyName, const FString& ClassName, UClass*& OutClass, FString& OutError)
{
    OutClass = nullptr;
    if (!Target || ClassName.IsEmpty())
    {
        return true;
    }

    FProperty* Property = Target->GetClass()->FindPropertyByName(FName(PropertyName));
    FClassProperty* ClassProperty = CastField<FClassProperty>(Property);
    if (!ClassProperty)
    {
        OutError = FString::Printf(TEXT("Class property '%s' was not found on '%s'."), PropertyName, *Target->GetClass()->GetName());
        return false;
    }

    UClass* Class = ResolveClassByName(ClassName);
    if (!Class)
    {
        Class = LoadObject<UClass>(nullptr, *ClassName);
    }
    if (!Class && ClassName.StartsWith(TEXT("/Script/")))
    {
        Class = FindObject<UClass>(nullptr, *ClassName);
    }
    if (!Class)
    {
        OutError = FString::Printf(TEXT("Could not resolve class '%s'."), *ClassName);
        return false;
    }
    if (ClassProperty->MetaClass && !Class->IsChildOf(ClassProperty->MetaClass))
    {
        OutError = FString::Printf(TEXT("Class '%s' is not assignable to '%s'."), *Class->GetName(), *ClassProperty->MetaClass->GetName());
        return false;
    }

    OutClass = Class;
    return true;
}

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

UWorld* GetPCGEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

AActor* FindPCGActor(UWorld* World, const FString& ActorName)
{
    if (!World || ActorName.IsEmpty())
    {
        return nullptr;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && (Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase) ||
            Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)))
        {
            return Actor;
        }
    }

    return nullptr;
}

UPCGComponent* FindPCGComponentOnActor(AActor* Actor, const FString& ComponentName)
{
    if (!Actor)
    {
        return nullptr;
    }

    TArray<UPCGComponent*> Components;
    Actor->GetComponents<UPCGComponent>(Components);
    for (UPCGComponent* Component : Components)
    {
        if (!Component)
        {
            continue;
        }
        const bool bIdentifierLooksLikePath = ComponentName.Contains(TEXT(".")) || ComponentName.Contains(TEXT("/"));
        if (ComponentName.IsEmpty() || Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase) ||
            Component->GetFName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase) ||
            Component->GetPathName().Equals(ComponentName, ESearchCase::IgnoreCase) ||
            Component->GetFullName().Equals(ComponentName, ESearchCase::IgnoreCase) ||
            (bIdentifierLooksLikePath && Component->GetPathName().EndsWith(ComponentName, ESearchCase::IgnoreCase)))
        {
            return Component;
        }
    }

    return nullptr;
}

UPCGComponent* FindPCGComponent(UWorld* World, const FString& ActorName, const FString& ComponentName, AActor*& OutActor)
{
    OutActor = nullptr;
    if (!World)
    {
        return nullptr;
    }

    if (!ActorName.IsEmpty())
    {
        OutActor = FindPCGActor(World, ActorName);
        return FindPCGComponentOnActor(OutActor, ComponentName);
    }

    if (ComponentName.IsEmpty())
    {
        return nullptr;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (UPCGComponent* Component = FindPCGComponentOnActor(Actor, ComponentName))
        {
            OutActor = Actor;
            return Component;
        }
    }

    return nullptr;
}

bool HasPCGComponentSelector(const FString& ActorName, const FString& ComponentName)
{
    return !ActorName.IsEmpty() || !ComponentName.IsEmpty();
}

UPCGComponent* CreatePCGComponent(AActor* Actor, const FString& ComponentName)
{
    if (!Actor)
    {
        return nullptr;
    }

    Actor->Modify();
    const FName NewComponentName = ComponentName.IsEmpty() ? NAME_None : FName(*ComponentName);
    UPCGComponent* Component = NewObject<UPCGComponent>(Actor, NewComponentName, RF_Transactional);
    if (!Component)
    {
        return nullptr;
    }

    Actor->AddInstanceComponent(Component);
    Component->RegisterComponent();
    Component->Modify();
    return Component;
}

bool SaveEditorWorldIfRequested(UWorld* World, bool bSave, bool& bOutSaved, FString& OutError)
{
    bOutSaved = false;
    if (!World)
    {
        OutError = TEXT("Could not resolve the editor world for level save.");
        return false;
    }

    if (!bSave)
    {
        return true;
    }

    if (!World->PersistentLevel)
    {
        OutError = TEXT("Could not resolve the persistent level for PCG save.");
        return false;
    }

    UPackage* WorldPackage = World->GetOutermost();
    const FString LevelPath = WorldPackage ? WorldPackage->GetName() : FString();
    if (LevelPath.IsEmpty())
    {
        OutError = TEXT("Could not resolve the current level package path for PCG save.");
        return false;
    }

    World->Modify();
    World->MarkPackageDirty();
    World->PersistentLevel->Modify();
    World->PersistentLevel->MarkPackageDirty();

    bOutSaved = McpSafeLevelSave(World->PersistentLevel, LevelPath);
    if (!bOutSaved)
    {
        OutError = FString::Printf(TEXT("Failed to save current level '%s' after PCG change."), *LevelPath);
        return false;
    }

    return true;
}

void ApplyNodeMetadata(UPCGNode* Node, const TSharedPtr<FJsonObject>& Payload)
{
    if (!Node || !Payload.IsValid())
    {
        return;
    }

    const FString Title = GetFirstStringField(Payload, {TEXT("nodeName"), TEXT("title"), TEXT("name")});
    if (!Title.IsEmpty())
    {
        Node->NodeTitle = FName(*Title);
    }

    double X = 0.0;
    double Y = 0.0;
    const bool bHasX = Payload->TryGetNumberField(TEXT("x"), X) || Payload->TryGetNumberField(TEXT("posX"), X);
    const bool bHasY = Payload->TryGetNumberField(TEXT("y"), Y) || Payload->TryGetNumberField(TEXT("posY"), Y);
    if (bHasX || bHasY)
    {
        Node->SetNodePosition(static_cast<int32>(X), static_cast<int32>(Y));
    }
}

bool SaveGraphIfRequested(UPCGGraph* Graph, bool bSave, bool& bOutSaved, FString& OutError)
{
    bOutSaved = false;
    if (!Graph)
    {
        OutError = TEXT("PCG graph is invalid.");
        return false;
    }

    Graph->PostEditChange();
    Graph->MarkPackageDirty();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
    Graph->ForceNotificationForEditor(EPCGChangeType::Structural);
#else
    Graph->NotifyGraphChanged(EPCGChangeType::Structural);
#endif

    if (bSave)
    {
        bOutSaved = McpSafeAssetSave(Graph);
        if (!bOutSaved)
        {
            OutError = FString::Printf(TEXT("Failed to save PCG graph '%s'."), *Graph->GetPathName());
            return false;
        }
    }

    return true;
}
}
#endif

bool UMcpAutomationBridgeSubsystem::HandleManagePCGAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (Action != TEXT("manage_pcg"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("manage_pcg requires an editor build."), TEXT("EDITOR_ONLY"));
    return true;
#elif !MCP_HAS_PCG
    SendAutomationError(Socket, RequestId, TEXT("PCG plugin support is not available in this build."), TEXT("PCG_PLUGIN_NOT_AVAILABLE"));
    return true;
#else
    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    if (!FModuleManager::Get().IsModuleLoaded(TEXT("PCG")) &&
        (!FModuleManager::Get().ModuleExists(TEXT("PCG")) || !FModuleManager::Get().LoadModulePtr<IModuleInterface>(TEXT("PCG"))))
    {
        SendAutomationError(Socket, RequestId, TEXT("PCG plugin is not enabled in this project."), TEXT("PCG_PLUGIN_NOT_ENABLED"));
        return true;
    }

    const FString SubAction = NormalizePCGSubAction(Payload);
    if (SubAction.IsEmpty() || !McpConsolidatedActions::IsPCGAction(SubAction))
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown PCG subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
        return true;
    }

    const bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

    if (SubAction == TEXT("create_pcg_graph"))
    {
        FString GraphPath;
        FString Error;
        if (!TryGetPCGAssetPath(Payload, {TEXT("graphPath"), TEXT("assetPath")}, GraphPath, Error))
        {
            SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        const bool bOverwrite = GetJsonBoolField(Payload, TEXT("overwrite"), false);
        bool bCreated = false;
        bool bSaved = false;
        UPCGGraph* Graph = CreateOrReusePCGGraph(GraphPath, bOverwrite, bSave, bCreated, bSaved, Error);
        if (!Graph)
        {
            SendAutomationError(Socket, RequestId, Error, bOverwrite ? TEXT("CREATE_FAILED") : TEXT("ASSET_ALREADY_EXISTS"));
            return true;
        }

        SendAutomationResponse(Socket, RequestId, true, bCreated ? TEXT("PCG graph created.") : TEXT("PCG graph already exists."), BuildGraphResult(Graph, GraphPath, bCreated, bSaved));
        return true;
    }

    if (SubAction == TEXT("create_pcg_subgraph"))
    {
        FString SubgraphPath;
        FString Error;
        if (!TryGetPCGAssetPath(Payload, {TEXT("subgraphPath"), TEXT("graphPath"), TEXT("assetPath")}, SubgraphPath, Error))
        {
            SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        const FString ParentGraphRawPath = GetJsonStringField(Payload, TEXT("parentGraphPath"));
        FString ParentGraphPath;
        UPCGGraph* ParentGraph = nullptr;
        if (!ParentGraphRawPath.IsEmpty())
        {
            ParentGraph = LoadPCGGraph(ParentGraphRawPath, ParentGraphPath, Error);
            if (!ParentGraph)
            {
                SendAutomationError(Socket, RequestId, Error, TEXT("ASSET_NOT_FOUND"));
                return true;
            }
        }

        const bool bOverwrite = GetJsonBoolField(Payload, TEXT("overwrite"), false);
        bool bCreated = false;
        bool bSubgraphSaved = false;
        UPCGGraph* Subgraph = CreateOrReusePCGGraph(SubgraphPath, bOverwrite, bSave, bCreated, bSubgraphSaved, Error);
        if (!Subgraph)
        {
            SendAutomationError(Socket, RequestId, Error, bOverwrite ? TEXT("CREATE_FAILED") : TEXT("ASSET_ALREADY_EXISTS"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = BuildGraphResult(Subgraph, SubgraphPath, bCreated, bSubgraphSaved);
        Result->SetStringField(TEXT("subgraphPath"), SubgraphPath);

        if (ParentGraph)
        {
            UPCGSettings* DefaultSettings = nullptr;
            UPCGNode* Node = ParentGraph->AddNodeOfType(UPCGSubgraphSettings::StaticClass(), DefaultSettings);
            UPCGBaseSubgraphSettings* SubgraphSettings = Cast<UPCGBaseSubgraphSettings>(DefaultSettings);
            if (!Node || !SubgraphSettings)
            {
                SendAutomationError(Socket, RequestId, TEXT("Failed to create PCG subgraph node."), TEXT("CREATE_FAILED"));
                return true;
            }

            SubgraphSettings->SetSubgraph(Subgraph);
            ApplyNodeMetadata(Node, Payload);
            Node->UpdateAfterSettingsChangeDuringCreation();
            SubgraphSettings->PostEditChange();
            bool bParentSaved = false;
            if (!SaveGraphIfRequested(ParentGraph, bSave, bParentSaved, Error))
            {
                SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
                return true;
            }

            Result->SetStringField(TEXT("parentGraphPath"), ParentGraphPath);
            Result->SetStringField(TEXT("nodeId"), Node->GetName());
            Result->SetStringField(TEXT("nodeName"), Node->GetName());
            Result->SetBoolField(TEXT("parentSaved"), bParentSaved);
        }

        SendAutomationResponse(Socket, RequestId, true, TEXT("PCG subgraph created."), Result);
        return true;
    }

    if (SubAction == TEXT("execute_pcg_graph"))
    {
        UWorld* World = GetPCGEditorWorld();
        if (!World)
        {
            SendAutomationError(Socket, RequestId, TEXT("Could not resolve the editor world for PCG execution."), TEXT("WORLD_NOT_FOUND"));
            return true;
        }

        FString Error;
        FString GraphPath;
        UPCGGraph* Graph = nullptr;
        const FString GraphRawPath = GetFirstStringField(Payload, {TEXT("graphPath"), TEXT("assetPath")});
        if (!GraphRawPath.IsEmpty())
        {
            Graph = LoadPCGGraph(GraphRawPath, GraphPath, Error);
            if (!Graph)
            {
                SendAutomationError(Socket, RequestId, Error, TEXT("ASSET_NOT_FOUND"));
                return true;
            }
        }

        const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
        const FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
        const FString ComponentPath = GetJsonStringField(Payload, TEXT("componentPath"));
        const FString ComponentSelector = !ComponentPath.IsEmpty() ? ComponentPath : ComponentName;
        const bool bCreateComponent = GetJsonBoolField(Payload, TEXT("createComponent"), false);
        AActor* Actor = nullptr;
        UPCGComponent* Component = FindPCGComponent(World, ActorName, ComponentSelector, Actor);
        if (!Component && !bCreateComponent && !HasPCGComponentSelector(ActorName, ComponentSelector))
        {
            SendAutomationError(Socket, RequestId, TEXT("execute_pcg_graph requires actorName, componentName, or componentPath when createComponent is false."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (!Component && bCreateComponent)
        {
            Actor = FindPCGActor(World, ActorName);
            if (!Actor)
            {
                SendAutomationError(Socket, RequestId, TEXT("createComponent requires an existing actorName."), TEXT("ACTOR_NOT_FOUND"));
                return true;
            }
            Component = CreatePCGComponent(Actor, ComponentName);
        }
        if (!Component)
        {
            SendAutomationError(Socket, RequestId, TEXT("Could not resolve a PCG component. Provide actorName/componentName, or actorName with createComponent=true."), TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }

        Component->Modify();
        if (Graph)
        {
            Component->SetGraphLocal(Graph);
        }

        const bool bForceGenerate = GetJsonBoolField(Payload, TEXT("force"), true);
        const FPCGTaskId TaskId = Component->GenerateLocalGetTaskId(bForceGenerate);
        if (TaskId == InvalidPCGTaskId)
        {
            SendAutomationError(Socket, RequestId, TEXT("PCG generation was not scheduled. The component may already be generating, be up to date with force=false, have no graph, or have invalid bounds."), TEXT("GENERATION_NOT_SCHEDULED"));
            return true;
        }

        bool bLevelSaved = false;
        if (!SaveEditorWorldIfRequested(World, bSave, bLevelSaved, Error))
        {
            SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("graphPath"), GraphPath);
        Result->SetStringField(TEXT("actorName"), Actor ? Actor->GetName() : FString());
        Result->SetStringField(TEXT("componentName"), Component->GetName());
        Result->SetStringField(TEXT("componentPath"), Component->GetPathName());
        Result->SetNumberField(TEXT("taskId"), static_cast<double>(TaskId));
        Result->SetBoolField(TEXT("force"), bForceGenerate);
        Result->SetBoolField(TEXT("saved"), bLevelSaved);
        McpHandlerUtils::AddVerification(Result, Component);
        SendAutomationResponse(Socket, RequestId, true, TEXT("PCG graph generation started."), Result);
        return true;
    }

    if (SubAction == TEXT("set_pcg_partition_grid_size"))
    {
        UWorld* World = GetPCGEditorWorld();
        if (!World)
        {
            SendAutomationError(Socket, RequestId, TEXT("Could not resolve the editor world for PCG partition grid size."), TEXT("WORLD_NOT_FOUND"));
            return true;
        }

        const int32 GridSize = GetJsonIntField(Payload, TEXT("gridSize"), 0);
        if (GridSize <= 0)
        {
            SendAutomationError(Socket, RequestId, TEXT("set_pcg_partition_grid_size requires a positive gridSize."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Scope = TEXT("world");
        if (Payload.IsValid() && Payload->HasField(TEXT("scope")) && !Payload->TryGetStringField(TEXT("scope"), Scope))
        {
            SendAutomationError(Socket, RequestId, TEXT("set_pcg_partition_grid_size scope must be a string: 'world' or 'component'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Scope = Scope.ToLower();
        if (Scope != TEXT("world") && Scope != TEXT("component"))
        {
            SendAutomationError(Socket, RequestId, TEXT("set_pcg_partition_grid_size scope must be 'world' or 'component'."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (Scope == TEXT("component"))
        {
            const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
            const FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
            const FString ComponentPath = GetJsonStringField(Payload, TEXT("componentPath"));
            const FString ComponentSelector = !ComponentPath.IsEmpty() ? ComponentPath : ComponentName;
            if (!HasPCGComponentSelector(ActorName, ComponentSelector))
            {
                SendAutomationError(Socket, RequestId, TEXT("component-scoped partition grid size requires actorName, componentName, or componentPath."), TEXT("INVALID_ARGUMENT"));
                return true;
            }
            AActor* Actor = nullptr;
            UPCGComponent* Component = FindPCGComponent(World, ActorName, ComponentSelector, Actor);
            if (!Component)
            {
                SendAutomationError(Socket, RequestId, TEXT("Could not resolve a PCG component for component-scoped grid size."), TEXT("COMPONENT_NOT_FOUND"));
                return true;
            }

            const uint32 PreviousGridSize = Component->GetGenerationGridSize();
            Component->Modify();
            Component->SetGenerationGridSize(static_cast<uint32>(GridSize));
            Component->PostEditChange();

            FString Error;
            bool bLevelSaved = false;
            if (!SaveEditorWorldIfRequested(World, bSave, bLevelSaved, Error))
            {
                SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
                return true;
            }

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("scope"), TEXT("component"));
            Result->SetStringField(TEXT("actorName"), Actor ? Actor->GetName() : FString());
            Result->SetStringField(TEXT("componentName"), Component->GetName());
            Result->SetStringField(TEXT("componentPath"), Component->GetPathName());
            Result->SetNumberField(TEXT("previousGridSize"), PreviousGridSize);
            Result->SetNumberField(TEXT("gridSize"), GridSize);
            Result->SetBoolField(TEXT("saved"), bLevelSaved);
            McpHandlerUtils::AddVerification(Result, Component);
            SendAutomationResponse(Socket, RequestId, true, TEXT("PCG component generation grid size updated."), Result);
            return true;
        }

        APCGWorldActor* PCGWorldActor = PCGHelpers::GetPCGWorldActor(World);
        if (!PCGWorldActor)
        {
            SendAutomationError(Socket, RequestId, TEXT("Could not resolve or create the PCG world actor."), TEXT("PCG_WORLD_ACTOR_NOT_FOUND"));
            return true;
        }

        const uint32 PreviousGridSize = PCGWorldActor->PartitionGridSize;
        FProperty* PartitionGridSizeProperty = FindFProperty<FProperty>(APCGWorldActor::StaticClass(), GET_MEMBER_NAME_CHECKED(APCGWorldActor, PartitionGridSize));
        PCGWorldActor->Modify();
        if (PartitionGridSizeProperty)
        {
            PCGWorldActor->PreEditChange(PartitionGridSizeProperty);
        }
        PCGWorldActor->PartitionGridSize = static_cast<uint32>(GridSize);
        if (PartitionGridSizeProperty)
        {
            FPropertyChangedEvent PropertyChangedEvent(PartitionGridSizeProperty, EPropertyChangeType::ValueSet);
            PCGWorldActor->PostEditChangeProperty(PropertyChangedEvent);
        }
        else
        {
            PCGWorldActor->PostEditChange();
        }

        FString Error;
        bool bLevelSaved = false;
        if (!SaveEditorWorldIfRequested(World, bSave, bLevelSaved, Error))
        {
            SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("scope"), TEXT("world"));
        Result->SetNumberField(TEXT("previousGridSize"), PreviousGridSize);
        Result->SetNumberField(TEXT("gridSize"), PCGWorldActor->PartitionGridSize);
        Result->SetBoolField(TEXT("saved"), bLevelSaved);
        McpHandlerUtils::AddVerification(Result, PCGWorldActor);
        SendAutomationResponse(Socket, RequestId, true, TEXT("PCG partition grid size updated."), Result);
        return true;
    }

    FString GraphRawPath = GetFirstStringField(Payload, {TEXT("graphPath"), TEXT("assetPath")});
    if (GraphRawPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'graphPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString GraphPath;
    FString Error;
    UPCGGraph* Graph = LoadPCGGraph(GraphRawPath, GraphPath, Error);
    if (!Graph)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    if (SubAction == TEXT("add_pcg_node") || IsPCGNodeCreationAction(SubAction))
    {
        FString NodeType = GetFirstStringField(Payload, {TEXT("settingsClass"), TEXT("nodeType")});
        if (NodeType.IsEmpty())
        {
            NodeType = SubAction;
        }
        UClass* SettingsClass = ResolvePCGSettingsClass(NodeType);
        if (!SettingsClass)
        {
            SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not resolve PCG settings class '%s'."), *NodeType), TEXT("CLASS_NOT_FOUND"));
            return true;
        }

        TSubclassOf<UPCGSettings> SettingsSubclass;
        SettingsSubclass = SettingsClass;
        UPCGSettings* DefaultSettings = nullptr;
        UPCGNode* Node = Graph->AddNodeOfType(SettingsSubclass, DefaultSettings);
        if (!Node || !DefaultSettings)
        {
            SendAutomationError(Socket, RequestId, TEXT("Failed to add PCG node."), TEXT("CREATE_FAILED"));
            return true;
        }

        const TSharedPtr<FJsonObject>* SettingsObject = nullptr;
        int32 AppliedSettings = 0;
        if (Payload->TryGetObjectField(TEXT("settings"), SettingsObject) && SettingsObject && SettingsObject->IsValid())
        {
            if (!ApplySettingsObject(DefaultSettings, *SettingsObject, Error, AppliedSettings))
            {
                SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
                return true;
            }
        }

        int32 AppliedConvenienceSettings = 0;
        if (!ApplyPCGConvenienceSettings(SubAction, DefaultSettings, Payload, Error, AppliedConvenienceSettings))
        {
            SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
            return true;
        }

        ApplyNodeMetadata(Node, Payload);
        Node->UpdateAfterSettingsChangeDuringCreation();
        DefaultSettings->PostEditChange();

        bool bSaved = false;
        if (!SaveGraphIfRequested(Graph, bSave, bSaved, Error))
        {
            SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = BuildNodeResult(Graph, Node, GraphPath);
        Result->SetNumberField(TEXT("settingsApplied"), AppliedSettings + AppliedConvenienceSettings);
        Result->SetBoolField(TEXT("saved"), bSaved);
        SendAutomationResponse(Socket, RequestId, true, TEXT("PCG node added."), Result);
        return true;
    }

    if (SubAction == TEXT("connect_pcg_pins"))
    {
        const FString SourceNodeId = GetJsonStringField(Payload, TEXT("sourceNodeId"));
        const FString TargetNodeId = GetJsonStringField(Payload, TEXT("targetNodeId"));
        if (SourceNodeId.IsEmpty() || TargetNodeId.IsEmpty())
        {
            SendAutomationError(Socket, RequestId, TEXT("connect_pcg_pins requires sourceNodeId and targetNodeId."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UPCGNode* SourceNode = FindPCGNode(Graph, SourceNodeId);
        UPCGNode* TargetNode = FindPCGNode(Graph, TargetNodeId);
        if (!SourceNode || !TargetNode)
        {
            SendAutomationError(Socket, RequestId, TEXT("Could not resolve source or target PCG node."), TEXT("NODE_NOT_FOUND"));
            return true;
        }

        const FString SourcePinLabel = GetFirstStringField(Payload, {TEXT("sourcePin"), TEXT("outputName")});
        const FString TargetPinLabel = GetFirstStringField(Payload, {TEXT("targetPin"), TEXT("inputName")});
        FName SourcePin;
        FName TargetPin;
        if (!TryResolvePCGPinLabel(SourceNode, true, SourcePinLabel, SourcePin, Error) ||
            !TryResolvePCGPinLabel(TargetNode, false, TargetPinLabel, TargetPin, Error))
        {
            SendAutomationError(Socket, RequestId, Error, TEXT("PIN_NOT_FOUND"));
            return true;
        }

        UPCGPin* SourcePinObject = SourceNode->GetOutputPin(SourcePin);
        UPCGPin* TargetPinObject = TargetNode->GetInputPin(TargetPin);
        if (!HasPCGEdge(SourcePinObject, TargetPinObject))
        {
            Graph->AddLabeledEdge(SourceNode, SourcePin, TargetNode, TargetPin);
        }

        if (!HasPCGEdge(SourcePinObject, TargetPinObject))
        {
            SendAutomationError(Socket, RequestId, TEXT("Failed to connect PCG pins."), TEXT("CONNECT_FAILED"));
            return true;
        }

        bool bSaved = false;
        if (!SaveGraphIfRequested(Graph, bSave, bSaved, Error))
        {
            SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("graphPath"), GraphPath);
        Result->SetStringField(TEXT("sourceNodeId"), SourceNode->GetName());
        Result->SetStringField(TEXT("targetNodeId"), TargetNode->GetName());
        Result->SetStringField(TEXT("sourcePin"), SourcePin.ToString());
        Result->SetStringField(TEXT("targetPin"), TargetPin.ToString());
        Result->SetBoolField(TEXT("saved"), bSaved);
        McpHandlerUtils::AddVerification(Result, Graph);
        SendAutomationResponse(Socket, RequestId, true, TEXT("PCG pins connected."), Result);
        return true;
    }

    if (SubAction == TEXT("set_pcg_node_settings"))
    {
        const FString NodeId = GetFirstStringField(Payload, {TEXT("nodeId"), TEXT("nodeName")});
        UPCGNode* Node = FindPCGNode(Graph, NodeId);
        if (!Node)
        {
            SendAutomationError(Socket, RequestId, TEXT("Could not resolve PCG node."), TEXT("NODE_NOT_FOUND"));
            return true;
        }

        UPCGSettings* Settings = Node->GetSettings();
        if (!Settings)
        {
            SendAutomationError(Socket, RequestId, TEXT("PCG node has no editable settings."), TEXT("SETTINGS_NOT_FOUND"));
            return true;
        }

        int32 AppliedSettings = 0;
        const TSharedPtr<FJsonObject>* SettingsObject = nullptr;
        if (Payload->TryGetObjectField(TEXT("settings"), SettingsObject) && SettingsObject && SettingsObject->IsValid())
        {
            if (!ApplySettingsObject(Settings, *SettingsObject, Error, AppliedSettings))
            {
                SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
                return true;
            }
        }

        int32 AppliedConvenienceSettings = 0;
        if (!ApplyPCGConvenienceSettings(SubAction, Settings, Payload, Error, AppliedConvenienceSettings))
        {
            SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
            return true;
        }

        ApplyNodeMetadata(Node, Payload);
        Node->UpdateAfterSettingsChangeDuringCreation();
        Settings->PostEditChange();

        bool bSaved = false;
        if (!SaveGraphIfRequested(Graph, bSave, bSaved, Error))
        {
            SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = BuildNodeResult(Graph, Node, GraphPath);
        Result->SetNumberField(TEXT("settingsApplied"), AppliedSettings + AppliedConvenienceSettings);
        Result->SetBoolField(TEXT("saved"), bSaved);
        SendAutomationResponse(Socket, RequestId, true, TEXT("PCG node settings updated."), Result);
        return true;
    }

    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unhandled PCG subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;
#endif
}

// Copyright (c) 2024 MCP Automation Bridge Contributors

// control_actor find, findBy=mesh and findBy=material. find answered by class and by name only, so the actors drawing
// one mesh, or showing one material in any slot, could not be asked for.

#include "Domains/ControlActor/Find/McpAutomationBridge_FindByAsset.h"
#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace McpFindByAsset
{
namespace
{
constexpr int32 DefaultLimit = 200;
constexpr int32 MaxLimit = 1000;

// A slot's material, looking through a dynamic instance made from it: a game that tints a material at runtime holds a
// UMaterialInstanceDynamic, and the asset it was made from is what the caller names.
UMaterialInterface* AssetBehind(UMaterialInterface* Material)
{
    while (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material))
    {
        Material = Dynamic->Parent;
    }
    return Material;
}

// Every actor of the world with a component Matches accepts. Matches adds what its find reports to the entry it is given
// (the component's name and class are already there) and answers whether the component matched. The first Limit actors
// are listed; Total counts them all, and a cut list says so.
bool Scan(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
          TSharedPtr<FMcpBridgeWebSocket> Socket, const TCHAR* AssetKey, const FString& AssetPath, const TCHAR* Noun,
          TFunctionRef<bool(UPrimitiveComponent*, const TSharedPtr<FJsonObject>&)> Matches)
{
    double Requested = 0.0;
    const int32 Limit = Payload->TryGetNumberField(TEXT("limit"), Requested)
        ? FMath::Clamp(static_cast<int32>(Requested), 1, MaxLimit) : DefaultLimit;
    // The Play In Editor world while a session runs, as find_by_class and list do: the editor world holds the originals,
    // not the actors that are playing.
    UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
    TArray<TSharedPtr<FJsonValue>> Actors;
    int32 Total = 0;
    if (World)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            TArray<UPrimitiveComponent*> Components;
            if (Actor)
            {
                Actor->GetComponents(Components);
            }
            TArray<TSharedPtr<FJsonValue>> Matched;
            for (UPrimitiveComponent* Component : Components)
            {
                TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("name"), Component->GetName());
                Entry->SetStringField(TEXT("class"), Component->GetClass()->GetName());
                if (Matches(Component, Entry))
                {
                    Matched.Add(MakeShared<FJsonValueObject>(Entry));
                }
            }
            if (Matched.Num() == 0)
            {
                continue;
            }
            ++Total;
            if (Actors.Num() >= Limit)
            {
                continue;
            }
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("label"), Actor->GetActorLabel());
            Row->SetStringField(TEXT("name"), Actor->GetName());
            Row->SetStringField(TEXT("path"), Actor->GetPathName());
            Row->SetStringField(TEXT("class"), Actor->GetClass()->GetPathName());
            Row->SetArrayField(TEXT("components"), Matched);
            Actors.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(AssetKey, AssetPath);
    Data->SetNumberField(TEXT("count"), Actors.Num());
    Data->SetArrayField(TEXT("actors"), Actors);
    Data->SetStringField(TEXT("worldSearched"), World ? World->GetName() : FString());
    if (Total > Actors.Num())
    {
        Data->SetBoolField(TEXT("truncated"), true);
        Data->SetNumberField(TEXT("totalCount"), Total);
    }
    const FString Cut = Total > Actors.Num() ? FString::Printf(TEXT(" of %d (limit %d)"), Total, Limit) : FString();
    Bridge->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d actors%s using the %s %s"), Actors.Num(), *Cut, Noun, *AssetPath), Data);
    return true;
}
}

bool HandleFindByMesh(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString MeshPath;
    Payload->TryGetStringField(TEXT("meshPath"), MeshPath);
    if (MeshPath.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("meshPath is required."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FString SafePath = SanitizeProjectRelativePath(MeshPath);
    UStaticMesh* Mesh = SafePath.IsEmpty() ? nullptr : Cast<UStaticMesh>(McpLoadAsset(SafePath));
    if (!Mesh)
    {
        Bridge->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("meshPath '%s' did not load as a static mesh, so no actors could be matched."), *MeshPath),
            TEXT("MESH_NOT_FOUND"));
        return true;
    }
    return Scan(Bridge, RequestId, Payload, Socket, TEXT("meshPath"), Mesh->GetPathName(), TEXT("mesh"),
        [Mesh](UPrimitiveComponent* Component, const TSharedPtr<FJsonObject>& Entry)
        {
            const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
            if (!MeshComponent || MeshComponent->GetStaticMesh() != Mesh)
            {
                return false;
            }
            if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Component))
            {
                Entry->SetNumberField(TEXT("instances"), Instanced->GetInstanceCount());
            }
            return true;
        });
}

bool HandleFindByMaterial(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString MaterialPath;
    Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);
    if (MaterialPath.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("materialPath is required."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString ResolvedPath;
    FString LoadError;
    UMaterialInterface* Material = LoadMaterialForMcp(MaterialPath, ResolvedPath, LoadError);
    if (!Material)
    {
        Bridge->SendAutomationError(Socket, RequestId, LoadError, TEXT("MATERIAL_NOT_FOUND"));
        return true;
    }
    return Scan(Bridge, RequestId, Payload, Socket, TEXT("materialPath"), Material->GetPathName(), TEXT("material"),
        [Material](UPrimitiveComponent* Component, const TSharedPtr<FJsonObject>& Entry)
        {
            // GetMaterial answers a slot's override when it has one, else the default of the mesh.
            TArray<TSharedPtr<FJsonValue>> Slots;
            for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
            {
                if (AssetBehind(Component->GetMaterial(Slot)) == Material)
                {
                    Slots.Add(MakeShared<FJsonValueNumber>(Slot));
                }
            }
            if (Slots.Num() == 0)
            {
                return false;
            }
            Entry->SetArrayField(TEXT("slots"), Slots);
            return true;
        });
}
}

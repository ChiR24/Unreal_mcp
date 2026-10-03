// Copyright (c) 2024 MCP Automation Bridge Contributors
//
// process_asset process=mesh_collision. A mesh imported without simple collision lets pawns fall through every
// placement of it, and configure_mesh_collision reaches only dynamic mesh actors. This replaces a static mesh ASSET's
// simple collision with one shape fitted to its bounds (box, sphere or capsule), makes its render triangles the
// collision (complex), or removes it (none); then it rebuilds the physics of every placed copy and saves the mesh.

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/UObjectIterator.h"

namespace McpMeshCollision
{
namespace
{
// One shape fitted to the mesh's local bounds; complex and none add no shape.
void AddFittedShape(UBodySetup& BodySetup, const FBox& Bounds, const FString& Type)
{
    const FVector Extent = Bounds.GetExtent();
    if (Type == TEXT("box"))
    {
        FKBoxElem Box(static_cast<float>(2.0 * Extent.X), static_cast<float>(2.0 * Extent.Y), static_cast<float>(2.0 * Extent.Z));
        Box.Center = Bounds.GetCenter();
        BodySetup.AggGeom.BoxElems.Add(Box);
    }
    else if (Type == TEXT("sphere"))
    {
        // The largest half-extent: a round mesh's own radius, where the bounds sphere would stand off a ball by 73%.
        FKSphereElem Sphere(static_cast<float>(Extent.GetMax()));
        Sphere.Center = Bounds.GetCenter();
        BodySetup.AggGeom.SphereElems.Add(Sphere);
    }
    else if (Type == TEXT("capsule"))
    {
        // Upright along Z, as a character's capsule: the length is the straight part between the two caps.
        const double Radius = FMath::Max(Extent.X, Extent.Y);
        FKSphylElem Capsule(static_cast<float>(Radius), static_cast<float>(FMath::Max(0.0, 2.0 * (Extent.Z - Radius))));
        Capsule.Center = Bounds.GetCenter();
        BodySetup.AggGeom.SphylElems.Add(Capsule);
    }
}
} // namespace

bool HandleSetMeshCollision(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    auto Fail = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge->SendAutomationResponse(Socket, RequestId, false, Message, nullptr, Code);
        return true;
    };

    FString AssetPath;
    FString Type;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    Payload->TryGetStringField(TEXT("collisionType"), Type);
    Type = Type.ToLower();
    if (AssetPath.IsEmpty()) return Fail(TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
    const FString SafePath = SanitizeProjectRelativePath(AssetPath);
    if (SafePath.IsEmpty()) return Fail(McpPathRefusalMessage(TEXT("assetPath"), AssetPath), TEXT("SECURITY_VIOLATION"));
    if (Type != TEXT("box") && Type != TEXT("sphere") && Type != TEXT("capsule") && Type != TEXT("complex") && Type != TEXT("none"))
    {
        return Fail(FString::Printf(TEXT("collisionType '%s' is not box, sphere, capsule, complex or none"), *Type), TEXT("INVALID_ARGUMENT"));
    }
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    UObject* Asset = McpLoadAsset(SafePath);
    UStaticMesh* Mesh = Cast<UStaticMesh>(Asset);
    if (!Asset) return Fail(FString::Printf(TEXT("No mesh asset at %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
    if (!Mesh) return Fail(FString::Printf(TEXT("%s is a %s, not a static mesh"), *AssetPath, *Asset->GetClass()->GetName()), TEXT("TYPE_MISMATCH"));
    // Engine meshes are shared by the editor itself (the basic shapes, gizmos): an in-memory change would reach all of it.
    if (Asset->GetOutermost()->GetName().StartsWith(TEXT("/Engine/")))
    {
        return Fail(TEXT("Engine content is not changed: duplicate the mesh into /Game and set the copy's collision."), TEXT("ENGINE_CONTENT"));
    }

    Mesh->Modify();
    if (!Mesh->GetBodySetup()) Mesh->CreateBodySetup();
    UBodySetup* BodySetup = Mesh->GetBodySetup();
    if (!BodySetup) return Fail(TEXT("The mesh has no body setup and none could be made"), TEXT("COLLISION_FAILED"));
    BodySetup->Modify();
    BodySetup->AggGeom.EmptyElements();
    // complex: the render triangles answer every query; none: no shapes, and nothing stands in for the triangles either.
    BodySetup->CollisionTraceFlag = Type == TEXT("complex") ? CTF_UseComplexAsSimple : Type == TEXT("none") ? CTF_UseSimpleAsComplex : CTF_UseDefault;
    AddFittedShape(*BodySetup, Mesh->GetBoundingBox(), Type);
    // A body already built ignores CreatePhysicsMeshes until it is invalidated (the converted-mesh fall-through bug).
    BodySetup->InvalidatePhysicsData();
    BodySetup->CreatePhysicsMeshes();
    Mesh->CreateNavCollision(true);
    // As the static mesh editor does after a collision change: placed copies keep their old bodies until rebuilt.
    int32 Refreshed = 0;
    for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
    {
        if (It->GetStaticMesh() == Mesh && It->IsPhysicsStateCreated())
        {
            It->RecreatePhysicsState();
            ++Refreshed;
        }
    }
    Mesh->MarkPackageDirty();
    const bool bSaved = bSave && SaveLoadedAssetThrottled(Asset);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("assetPath"), SafePath);
    Resp->SetStringField(TEXT("collisionType"), Type);
    Resp->SetNumberField(TEXT("shapeCount"), BodySetup->AggGeom.GetElementCount());
    Resp->SetNumberField(TEXT("componentsRefreshed"), Refreshed);
    Resp->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Resp, Asset);
    if (bSave && !bSaved)
    {
        Bridge->SendAutomationResponse(Socket, RequestId, false,
                                       FString::Printf(TEXT("%s collides as %s but could not be saved"), *SafePath, *Type), Resp, TEXT("SAVE_FAILED"));
        return true;
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                                   FString::Printf(TEXT("%s now collides as %s (%d placed copies rebuilt)"), *SafePath, *Type, Refreshed), Resp);
    return true;
}
} // namespace McpMeshCollision

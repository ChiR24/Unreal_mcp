#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT
#include "PhysicsEngine/BodySetup.h"

namespace McpGeometryHandlers
{
// outputPath names the converted asset (dogfood #135). A folder
// (trailing '/' or an existing content folder) gets DefaultName appended; anything else
// is the full asset path. Everything passes the project path sanitizer first.
bool ResolveConversionAssetPath(const TSharedPtr<FJsonObject>& Payload, const FString& DefaultName,
                                FString& OutAssetPath, FString& OutError)
{
    const FString OutRequested = GetJsonStringField(Payload, TEXT("outputPath"));
    if (OutRequested.IsEmpty())
    {
        OutAssetPath = TEXT("/Game/GeneratedMeshes/") + DefaultName;
        return true;
    }
    const bool bExplicitFolder = OutRequested.EndsWith(TEXT("/"));
    FString Sanitized = SanitizeProjectRelativePath(OutRequested);
    if (Sanitized.IsEmpty())
    {
        OutError = McpPathRefusalMessage(TEXT("outputPath"), OutRequested);
        return false;
    }
    Sanitized = FPackageName::ObjectPathToPackageName(Sanitized);
    Sanitized.RemoveFromEnd(TEXT("/"));
    if (bExplicitFolder || UEditorAssetLibrary::DoesDirectoryExist(Sanitized))
    {
        Sanitized += TEXT("/") + DefaultName;
    }
    OutAssetPath = Sanitized;
    return true;
}

bool HandleConvertToStaticMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bNanite)
{
    // convert_to_nanite is the same bake with Nanite enabled.
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    FString AssetPath;
    FString PathError;
    if (!ResolveConversionAssetPath(Payload, bNanite ? ActorName + TEXT("_Nanite") : ActorName, AssetPath, PathError))
    {
        Self->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_ASSET_PATH"));
        return true;
    }

    FGeometryScriptCreateNewStaticMeshAssetOptions CreateOptions;
    CreateOptions.bEnableRecomputeNormals = true;
    CreateOptions.bEnableRecomputeTangents = true;
    CreateOptions.bEnableNanite = bNanite;
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
    // From 5.1 the new mesh takes these settings whole when Nanite is asked for, and their
    // bEnabled defaults to false, so bEnableNanite alone baked every "Nanite" mesh without it.
    CreateOptions.NaniteSettings.bEnabled = bNanite;
#endif

    EGeometryScriptOutcomePins Outcome;
    UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
        Target->Mesh,
        AssetPath,
        CreateOptions,
        Outcome,
        nullptr
    );

    if (Outcome != EGeometryScriptOutcomePins::Success)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to create StaticMesh asset"), TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    // A freshly created StaticMesh asset has NO collision body, so pawns fell
    // straight through any level geometry built from converted meshes even
    // though the asset itself rendered fine. Give the asset a simple collision
    // body derived from its bounds (exact for the box primitives, a tight
    // approximation for the round ones) and cook it synchronously, so the
    // converted mesh is standable in PIE without a separate round-trip.
    //
    // Built from explicit convex-hull vertices in body space via the
    // long-stable UBodySetup/FKAggregateGeom API rather than version-drifting
    // Geometry Script static-mesh collision helpers.
    UStaticMesh* CreatedMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *AssetPath));
    if (CreatedMesh)
    {
        UBodySetup* BodySetup = CreatedMesh->GetBodySetup();
        if (!BodySetup)
        {
            BodySetup = NewObject<UBodySetup>(CreatedMesh, NAME_None, RF_Transactional);
            CreatedMesh->SetBodySetup(BodySetup);
        }

        const FBox Bounds = CreatedMesh->GetBounds().GetBox();
        const FVector Min = Bounds.Min;
        const FVector Max = Bounds.Max;

        BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
        BodySetup->AggGeom.ConvexElems.Reset();
        BodySetup->AggGeom.BoxElems.Reset();
        BodySetup->AggGeom.SphereElems.Reset();
        BodySetup->AggGeom.SphylElems.Reset();
        BodySetup->AggGeom.TaperedCapsuleElems.Reset();

        FKConvexElem ConvexElem;
        ConvexElem.VertexData.Reset(8);
        for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            ConvexElem.VertexData.Add(FVector(
                (CornerIndex & 1) ? Max.X : Min.X,
                (CornerIndex & 2) ? Max.Y : Min.Y,
                (CornerIndex & 4) ? Max.Z : Min.Z));
        }
        ConvexElem.UpdateElemBox();
        BodySetup->AggGeom.ConvexElems.Add(ConvexElem);

        // Cook the collision data so PIE can stand on the mesh immediately
        // after this request returns.
        BodySetup->CreatePhysicsMeshes();
        CreatedMesh->MarkPackageDirty();
        // The asset was written before the body was added; without a second save the
        // collision was gone on the next editor load.
        if (!McpSafeAssetSave(CreatedMesh))
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(
                TEXT("%s was created, but its collision body could not be saved to disk."), *AssetPath), TEXT("SAVE_FAILED"));
            return true;
        }
    }

    // Read back from the mesh: the reply used to echo the request.
    bool bNaniteOn = false;
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 3
    bNaniteOn = CreatedMesh && CreatedMesh->IsNaniteEnabled();
#else
    bNaniteOn = CreatedMesh && CreatedMesh->NaniteSettings.bEnabled;
#endif
    if (bNanite && !bNaniteOn)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("%s was created, but Nanite is not enabled on it."), *AssetPath), TEXT("NANITE_NOT_ENABLED"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("naniteEnabled"), bNaniteOn);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("StaticMesh created from DynamicMesh"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT

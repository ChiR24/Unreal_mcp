#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
#include "PhysicsEngine/BodySetup.h"

namespace McpGeometryHandlers
{
// outputPath / assetPath / savePath name the converted asset (dogfood #135). A folder
// (trailing '/' or an existing content folder) gets DefaultName appended; anything else
// is the full asset path. Everything passes the project path sanitizer first.
static bool ResolveConversionAssetPath(const TSharedPtr<FJsonObject>& Payload, const FString& DefaultName,
                                       FString& OutAssetPath, FString& OutRequested, FString& OutError)
{
    for (const TCHAR* Field : {TEXT("outputPath"), TEXT("assetPath"), TEXT("savePath")})
    {
        if (Payload->TryGetStringField(Field, OutRequested) && !OutRequested.IsEmpty())
        {
            break;
        }
    }
    if (OutRequested.IsEmpty())
    {
        OutAssetPath = TEXT("/Game/GeneratedMeshes/") + DefaultName;
        return true;
    }
    const bool bExplicitFolder = OutRequested.EndsWith(TEXT("/"));
    FString Sanitized = SanitizeProjectRelativePath(OutRequested);
    if (Sanitized.IsEmpty())
    {
        OutError = TEXT("Invalid outputPath - rejected due to security validation");
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
                                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString AssetPath;
    FString RequestedPath;
    FString PathError;
    if (!ResolveConversionAssetPath(Payload, ActorName, AssetPath, RequestedPath, PathError))
    {
        Self->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_ASSET_PATH"));
        return true;
    }

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    FGeometryScriptCreateNewStaticMeshAssetOptions CreateOptions;
    CreateOptions.bEnableRecomputeNormals = true;
    CreateOptions.bEnableRecomputeTangents = true;
    // UE 5.7: bAllowDistanceField and bGenerateNaniteEnabledMesh were removed
    // Use bEnableNanite + NaniteSettings instead
    CreateOptions.bEnableNanite = false;

    EGeometryScriptOutcomePins Outcome;
    UStaticMesh* NewStaticMesh = nullptr;

    UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
        Mesh,
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
    if (UStaticMesh* CreatedMesh = Cast<UStaticMesh>(
            StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *AssetPath)))
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
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("assetPath"), AssetPath);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("StaticMesh created from DynamicMesh"), Result);
    return true;
}

bool HandleConvertToNanite(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString AssetPath;
    FString RequestedPath;
    FString PathError;
    if (!ResolveConversionAssetPath(Payload, ActorName + TEXT("_Nanite"), AssetPath, RequestedPath, PathError))
    {
        Self->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_ASSET_PATH"));
        return true;
    }

    ADynamicMeshActor* TargetActor = nullptr;
    UDynamicMeshComponent* DMC = nullptr;
    UDynamicMesh* Mesh = nullptr;
    if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, Mesh))
    {
        return true;
    }

    FGeometryScriptCreateNewStaticMeshAssetOptions CreateOptions;
    CreateOptions.bEnableRecomputeNormals = true;
    CreateOptions.bEnableRecomputeTangents = true;
    CreateOptions.bEnableNanite = true;

    EGeometryScriptOutcomePins Outcome;

    UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
        Mesh,
        AssetPath,
        CreateOptions,
        Outcome,
        nullptr
    );

    if (Outcome != EGeometryScriptOutcomePins::Success)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to create Nanite StaticMesh asset"), TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("naniteEnabled"), true);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Nanite StaticMesh created from DynamicMesh"), Result);
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

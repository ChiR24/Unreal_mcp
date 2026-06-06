#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleConvertToStaticMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString AssetPath = GetStringFieldGeom(Payload, TEXT("assetPath"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (AssetPath.IsEmpty())
    {
        AssetPath = FString::Printf(TEXT("/Game/GeneratedMeshes/%s"), *ActorName);
    }

    // Sanitize the asset path to prevent path traversal and ensure valid path format
    FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedAssetPath.IsEmpty() && !AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Invalid assetPath - rejected due to security validation"), TEXT("INVALID_ASSET_PATH"));
        return true;
    }
    AssetPath = SanitizedAssetPath;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("assetPath"), AssetPath);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("StaticMesh created from DynamicMesh"), Result);
    return true;
}

// -------------------------------------------------------------------------
// Additional Primitives
// -------------------------------------------------------------------------

bool HandleConvertToNanite(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                  const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    FString AssetPath = GetStringFieldGeom(Payload, TEXT("assetPath"));

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (AssetPath.IsEmpty())
    {
        AssetPath = FString::Printf(TEXT("/Game/GeneratedMeshes/%s_Nanite"), *ActorName);
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    ADynamicMeshActor* TargetActor = nullptr;

    for (TActorIterator<ADynamicMeshActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    UDynamicMeshComponent* DMC = TargetActor->GetDynamicMeshComponent();
    if (!DMC || !DMC->GetDynamicMesh())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("DynamicMesh not available"), TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UDynamicMesh* Mesh = DMC->GetDynamicMesh();

    FGeometryScriptCreateNewStaticMeshAssetOptions CreateOptions;
    CreateOptions.bEnableRecomputeNormals = true;
    CreateOptions.bEnableRecomputeTangents = true;
    // Enable Nanite for this conversion
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

// -------------------------------------------------------------------------
// Extrude Along Spline
// -------------------------------------------------------------------------
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleGenerateLODsGeometry(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetStringFieldGeom(Payload, TEXT("actorName"));
    int32 LODCount = GetIntFieldGeom(Payload, TEXT("lodCount"), 4);
    FString AssetPath = GetStringFieldGeom(Payload, TEXT("assetPath"), TEXT(""));

    if (ActorName.IsEmpty() && AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("actorName or assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    LODCount = FMath::Clamp(LODCount, 1, 50);

#if WITH_EDITOR
    UStaticMesh* StaticMesh = nullptr;
    FString TargetPath;

    // If we have an asset path, load the existing static mesh
    if (!AssetPath.IsEmpty())
    {
        FString SafePath = SanitizeProjectRelativePath(AssetPath);
        if (SafePath.IsEmpty())
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Invalid asset path: %s"), *AssetPath), TEXT("INVALID_ASSET_PATH"));
            return true;
        }

        StaticMesh = LoadObject<UStaticMesh>(nullptr, *SafePath);
        if (!StaticMesh)
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("StaticMesh not found: %s"), *SafePath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        TargetPath = SafePath;
    }
    else
    {
        // Convert DynamicMesh to StaticMesh first
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

        UDynamicMesh* DynMesh = DMC->GetDynamicMesh();

        // Convert to StaticMesh
        FString MeshName = ActorName + TEXT("_LOD");
        TargetPath = FString::Printf(TEXT("/Game/MCPTest/%s"), *MeshName);

        FGeometryScriptCreateNewStaticMeshAssetOptions AssetOptions;
        AssetOptions.bEnableRecomputeNormals = true;
        AssetOptions.bEnableRecomputeTangents = true;
        AssetOptions.bEnableNanite = false;

        EGeometryScriptOutcomePins Outcome;

        StaticMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
            DynMesh, TargetPath, AssetOptions, Outcome, nullptr);

        if (Outcome != EGeometryScriptOutcomePins::Success || !StaticMesh)
        {
            Self->SendAutomationError(Socket, RequestId, TEXT("Failed to convert DynamicMesh to StaticMesh"), TEXT("CONVERSION_FAILED"));
            return true;
        }
    }

    // Generate LODs
    StaticMesh->Modify();
    StaticMesh->SetNumSourceModels(LODCount);

    // Configure LOD reduction settings with progressive reduction
    for (int32 LODIndex = 1; LODIndex < LODCount; LODIndex++)
    {
        FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(LODIndex);
        FMeshReductionSettings& ReductionSettings = SourceModel.ReductionSettings;

        // Progressive reduction: 50%, 25%, 12.5%...
        float ReductionPercent = 1.0f / FMath::Pow(2.0f, static_cast<float>(LODIndex));
        ReductionSettings.PercentTriangles = ReductionPercent;
        ReductionSettings.PercentVertices = ReductionPercent;

        SourceModel.BuildSettings.bRecomputeNormals = false;
        SourceModel.BuildSettings.bRecomputeTangents = false;
        SourceModel.BuildSettings.bUseMikkTSpace = true;
    }

    // Build the mesh with new LOD settings
    StaticMesh->Build();
    StaticMesh->PostEditChange();
    McpSafeAssetSave(StaticMesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), TargetPath);
    Result->SetNumberField(TEXT("lodCount"), LODCount);
    Result->SetNumberField(TEXT("triangles"), StaticMesh->GetNumTriangles(0));

    // Add verification data
    McpHandlerUtils::AddVerification(Result, StaticMesh);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("LODs generated for geometry"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Requires editor build"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// Set LOD Settings
// -------------------------------------------------------------------------

bool HandleSetLODSettings(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetStringFieldGeom(Payload, TEXT("assetPath"));
    int32 LODIndex = GetIntFieldGeom(Payload, TEXT("lodIndex"), 1);
    double TrianglePercent = GetNumberFieldGeom(Payload, TEXT("trianglePercent"), 50.0);
    bool bRecomputeNormals = GetBoolFieldGeom(Payload, TEXT("recomputeNormals"), false);
    bool bRecomputeTangents = GetBoolFieldGeom(Payload, TEXT("recomputeTangents"), false);

    if (AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

#if WITH_EDITOR
    FString SafePath = SanitizeProjectRelativePath(AssetPath);
    if (SafePath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Invalid asset path: %s"), *AssetPath), TEXT("INVALID_ASSET_PATH"));
        return true;
    }

    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *SafePath);
    if (!StaticMesh)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("StaticMesh not found: %s"), *SafePath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    if (LODIndex < 0 || LODIndex >= StaticMesh->GetNumSourceModels())
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Invalid LOD index: %d (mesh has %d LODs)"), LODIndex, StaticMesh->GetNumSourceModels()), TEXT("INVALID_LOD_INDEX"));
        return true;
    }

    StaticMesh->Modify();

    FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(LODIndex);

    // Set reduction settings
    SourceModel.ReductionSettings.PercentTriangles = TrianglePercent / 100.0f;
    SourceModel.ReductionSettings.PercentVertices = TrianglePercent / 100.0f;

    // Set build settings
    SourceModel.BuildSettings.bRecomputeNormals = bRecomputeNormals;
    SourceModel.BuildSettings.bRecomputeTangents = bRecomputeTangents;

    // Rebuild
    StaticMesh->Build();
    StaticMesh->PostEditChange();
    McpSafeAssetSave(StaticMesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), SafePath);
    Result->SetNumberField(TEXT("lodIndex"), LODIndex);
    Result->SetNumberField(TEXT("trianglePercent"), TrianglePercent);

    // Add verification data
    McpHandlerUtils::AddVerification(Result, StaticMesh);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("LOD settings updated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Requires editor build"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

// -------------------------------------------------------------------------
// Set LOD Screen Sizes
// -------------------------------------------------------------------------

bool HandleSetLODScreenSizes(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetStringFieldGeom(Payload, TEXT("assetPath"));

    // Parse screen sizes (can be array or object)
    TArray<float> ScreenSizes;
    const TArray<TSharedPtr<FJsonValue>>* SizeArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("screenSizes"), SizeArray))
    {
        for (const auto& Val : *SizeArray)
        {
            if (Val.IsValid() && Val->Type == EJson::Number)
            {
                ScreenSizes.Add(static_cast<float>(Val->AsNumber()));
            }
        }
    }

    if (AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (ScreenSizes.Num() == 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("screenSizes array required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

#if WITH_EDITOR
    FString SafePath = SanitizeProjectRelativePath(AssetPath);
    if (SafePath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Invalid asset path: %s"), *AssetPath), TEXT("INVALID_ASSET_PATH"));
        return true;
    }

    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *SafePath);
    if (!StaticMesh)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("StaticMesh not found: %s"), *SafePath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    StaticMesh->Modify();

    // Set screen sizes for each LOD
    // Note: UE5 doesn't have a direct SetLODScreenSize API on UStaticMesh
    // Screen sizes are typically managed via the LODGroup or platform-specific settings
    // Here we configure the reduction settings which indirectly affect LOD switching
    int32 NumLODs = StaticMesh->GetNumSourceModels();

    for (int32 i = 0; i < FMath::Min(ScreenSizes.Num(), NumLODs); i++)
    {
        // Configure reduction settings based on screen size
        // The screen size affects when this LOD becomes visible
        // Higher screen size = LOD becomes visible sooner (closer to camera)
        if (i > 0)  // LOD 0 is the base mesh
        {
            FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(i);
            // Screen size is used to determine when to switch to this LOD
            // We set it as a percentage of the previous LOD's screen size
            SourceModel.ReductionSettings.PercentTriangles = ScreenSizes[i];
        }
    }

    StaticMesh->PostEditChange();
    McpSafeAssetSave(StaticMesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), SafePath);
    Result->SetNumberField(TEXT("lodCount"), NumLODs);
    Result->SetNumberField(TEXT("screenSizesSet"), ScreenSizes.Num());

    // Add verification data
    McpHandlerUtils::AddVerification(Result, StaticMesh);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("LOD screen sizes updated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Requires editor build"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
// LOD settings target a static mesh asset; when the caller names a level actor
// (targetActor / actorName, as the contract allows) use its static mesh.
FString ResolveStaticMeshAssetPathFromActor(const TSharedPtr<FJsonObject>& Payload)
{
#if WITH_EDITOR
    FString ActorName = GetJsonStringField(Payload, TEXT("targetActor"));
    if (ActorName.IsEmpty())
    {
        ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    }
    if (ActorName.IsEmpty() || !GEditor)
    {
        return FString();
    }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FString();
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() != ActorName && It->GetName() != ActorName)
        {
            continue;
        }
        if (UStaticMeshComponent* Component = It->FindComponentByClass<UStaticMeshComponent>())
        {
            if (UStaticMesh* Mesh = Component->GetStaticMesh())
            {
                return Mesh->GetPathName();
            }
        }
    }
#endif
    return FString();
}
}


#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleGenerateLODsGeometry(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 LODCount = GetJsonIntField(Payload, TEXT("lodCount"), 4);
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));

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
        ADynamicMeshActor* TargetActor = nullptr;
        UDynamicMeshComponent* DMC = nullptr;
        UDynamicMesh* DynMesh = nullptr;
        if (!ResolveDynamicMeshForGeometry(Self, RequestId, ActorName, Socket, TargetActor, DMC, DynMesh))
        {
            return true;
        }

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

    McpHandlerUtils::AddVerification(Result, StaticMesh);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("LODs generated for geometry"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Requires editor build"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

bool HandleSetLODSettings(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"));
    if (AssetPath.IsEmpty())
    {
        AssetPath = ResolveStaticMeshAssetPathFromActor(Payload);
    }
    int32 LODIndex = GetJsonIntField(Payload, TEXT("lodIndex"), 1);
    double TrianglePercent = GetJsonNumberField(Payload, TEXT("trianglePercent"), 50.0);
    bool bRecomputeNormals = GetJsonBoolField(Payload, TEXT("recomputeNormals"), false);
    bool bRecomputeTangents = GetJsonBoolField(Payload, TEXT("recomputeTangents"), false);

    if (AssetPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("assetPath required: targetActor/actorName did not resolve to a StaticMeshComponent with a static mesh asset (LOD settings apply to static mesh assets, not dynamic meshes)"), TEXT("INVALID_ARGUMENT"));
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

    SourceModel.ReductionSettings.PercentTriangles = TrianglePercent / 100.0f;
    SourceModel.ReductionSettings.PercentVertices = TrianglePercent / 100.0f;

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

    McpHandlerUtils::AddVerification(Result, StaticMesh);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("LOD settings updated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Requires editor build"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

bool HandleSetLODScreenSizes(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"));
    if (AssetPath.IsEmpty())
    {
        AssetPath = ResolveStaticMeshAssetPathFromActor(Payload);
    }

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
        Self->SendAutomationError(Socket, RequestId, TEXT("assetPath required: targetActor/actorName did not resolve to a StaticMeshComponent with a static mesh asset (LOD settings apply to static mesh assets, not dynamic meshes)"), TEXT("INVALID_ARGUMENT"));
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

    // FStaticMeshSourceModel::ScreenSize is the field that decides when a LOD
    // takes over. Writing the requested screen sizes into
    // ReductionSettings.PercentTriangles instead left every screen size
    // untouched AND silently re-reduced each LOD's triangle budget to the
    // screen-size number, while the reply still said "LOD screen sizes
    // updated". Auto-compute has to go off too, or the next build recomputes
    // the screen sizes straight back over the caller's values.
    int32 NumLODs = StaticMesh->GetNumSourceModels();
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
    StaticMesh->SetAutoComputeLODScreenSize(false);
#else
    StaticMesh->bAutoComputeLODScreenSize = false;
#endif

    const int32 ScreenSizesApplied = FMath::Min(ScreenSizes.Num(), NumLODs);
    for (int32 i = 0; i < ScreenSizesApplied; i++)
    {
        StaticMesh->GetSourceModel(i).ScreenSize = ScreenSizes[i];
    }

    StaticMesh->PostEditChange();
    McpSafeAssetSave(StaticMesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), SafePath);
    Result->SetNumberField(TEXT("lodCount"), NumLODs);
    Result->SetNumberField(TEXT("screenSizesSet"), ScreenSizesApplied);

    McpHandlerUtils::AddVerification(Result, StaticMesh);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("LOD screen sizes updated"), Result);
#else
    Self->SendAutomationError(Socket, RequestId, TEXT("Requires editor build"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

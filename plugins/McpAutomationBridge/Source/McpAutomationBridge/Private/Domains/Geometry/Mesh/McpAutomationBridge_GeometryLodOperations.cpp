#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
// LOD settings target a static mesh asset; the caller names the level actor that
// uses it (targetActor / actorName). Replies with the error and returns null otherwise.
UStaticMesh* ResolveLodMesh(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World && !ActorName.IsEmpty())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetActorLabel() != ActorName && It->GetName() != ActorName) continue;
            const UStaticMeshComponent* Component = It->FindComponentByClass<UStaticMeshComponent>();
            if (UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr) return Mesh;
        }
    }
    Self->SendAutomationError(Socket, RequestId, TEXT("targetActor/actorName did not resolve to a StaticMeshComponent with a static mesh asset (LOD settings apply to static mesh assets, not dynamic meshes)"), TEXT("INVALID_ARGUMENT"));
    return nullptr;
}
}

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool HandleGenerateLODsGeometry(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    // The record declares only the actor form: the dynamic mesh is baked to a static
    // mesh asset, which then gets progressive LODs.
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const int32 LODCount = FMath::Clamp(GetJsonIntField(Payload, TEXT("lodCount"), 4), 1, 50);
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;

    // The bake used to land in a hard-coded /Game/MCPTest folder the caller could
    // not change; it follows convert_to_static_mesh now (outputPath, else
    // /Game/GeneratedMeshes).
    FString TargetPath;
    FString PathError;
    if (!ResolveConversionAssetPath(Payload, ActorName + TEXT("_LOD"), TargetPath, PathError))
    {
        Self->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
        return true;
    }
    FGeometryScriptCreateNewStaticMeshAssetOptions AssetOptions;
    AssetOptions.bEnableRecomputeNormals = true;
    AssetOptions.bEnableRecomputeTangents = true;
    AssetOptions.bEnableNanite = false;
    EGeometryScriptOutcomePins Outcome;
    UStaticMesh* StaticMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
        Target->Mesh, TargetPath, AssetOptions, Outcome, nullptr);
    if (Outcome != EGeometryScriptOutcomePins::Success || !StaticMesh)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Failed to convert DynamicMesh to StaticMesh"), TEXT("CONVERSION_FAILED"));
        return true;
    }
    McpHandlerUtils::ApplyProgressiveLods(StaticMesh, LODCount);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), TargetPath);
    Result->SetNumberField(TEXT("lodCount"), LODCount);
    Result->SetNumberField(TEXT("triangles"), StaticMesh->GetNumTriangles(0));

    McpHandlerUtils::AddVerification(Result, StaticMesh);

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("LODs generated for geometry"), Result);
    return true;
}

bool HandleSetLODSettings(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                 const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    int32 LODIndex = GetJsonIntField(Payload, TEXT("lodIndex"), 1);
    double TrianglePercent = GetJsonNumberField(Payload, TEXT("trianglePercent"), 100.0 - GetJsonNumberField(Payload, TEXT("reductionPercent"), 50.0));
    bool bRecomputeNormals = GetJsonBoolField(Payload, TEXT("recomputeNormals"), false);
    bool bRecomputeTangents = GetJsonBoolField(Payload, TEXT("recomputeTangents"), false);

    UStaticMesh* StaticMesh = ResolveLodMesh(Self, RequestId, Payload, Socket);
    if (!StaticMesh) return true;
    const FString SafePath = StaticMesh->GetPathName();

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
    return true;
}

bool HandleSetLODScreenSizes(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
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

    if (ScreenSizes.Num() == 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("screenSizes array required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UStaticMesh* StaticMesh = ResolveLodMesh(Self, RequestId, Payload, Socket);
    if (!StaticMesh) return true;
    const FString SafePath = StaticMesh->GetPathName();

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
    return true;
}

} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
